/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <gui/system_workspace.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module editor.systems_workspace;

namespace epochengine::editor_systems
{
    namespace
    {
        namespace graph = authoring::task_graph;
        namespace workspace = gui_lib::system_workspace;

        [[nodiscard]] workspace::Limits workspace_limits(
            const ControllerLimits& limits) noexcept
        {
            workspace::Limits result{};
            result.maximum_rows = (std::max)(
                static_cast<std::size_t>(limits.maximum_registry_systems),
                (std::max)(
                    static_cast<std::size_t>(limits.maximum_learning_nodes),
                    static_cast<std::size_t>(limits.maximum_live_scheduler_nodes)));
            result.maximum_id_bytes = limits.maximum_name_bytes + 48u;
            result.maximum_label_bytes = limits.maximum_name_bytes;
            result.maximum_category_bytes = limits.maximum_category_bytes + 32u;
            result.maximum_filter_bytes = limits.maximum_filter_bytes;
            return result;
        }

        [[nodiscard]] graph::GraphLimits graph_limits(
            const ControllerLimits& limits) noexcept
        {
            graph::GraphLimits result{};
            result.maximum_active_nodes = limits.maximum_learning_nodes;
            result.maximum_node_slots = limits.maximum_learning_nodes * 4u;
            result.maximum_edges = limits.maximum_learning_edges;
            result.maximum_name_bytes = limits.maximum_name_bytes;
            result.maximum_category_bytes = limits.maximum_category_bytes;
            result.maximum_history_operations = limits.maximum_learning_history;
            return result;
        }

        [[nodiscard]] workspace::RowStatus system_status(
            const systems::RegistryDiagnosticSnapshot& snapshot,
            const systems::SystemDiagnostic& system) noexcept
        {
            if (system.lifecycle == systems::SystemLifecycle::stopped)
                return workspace::RowStatus::disabled;
            if (!snapshot.order_resolved)
                return workspace::RowStatus::waiting;
            if (system.lifecycle == systems::SystemLifecycle::registered)
                return workspace::RowStatus::idle;
            if (!snapshot.initialized)
                return workspace::RowStatus::waiting;
            return system.update_count == 0u
                ? workspace::RowStatus::idle
                : workspace::RowStatus::healthy;
        }

        [[nodiscard]] std::string duration_text(std::uint64_t nanoseconds)
        {
            const std::uint64_t whole = nanoseconds / 1'000'000u;
            const std::uint64_t fraction = (nanoseconds % 1'000'000u) / 1'000u;
            std::string result = std::to_string(whole);
            result += ".";
            if (fraction < 100u)
                result += "0";
            if (fraction < 10u)
                result += "0";
            result += std::to_string(fraction);
            result += " ms";
            return result;
        }

        void append_dependencies(
            std::string& detail,
            std::span<const std::string> dependencies)
        {
            detail += " | Depends: ";
            if (dependencies.empty())
            {
                detail += "none";
                return;
            }
            for (std::size_t index = 0u; index < dependencies.size(); ++index)
            {
                if (index != 0u)
                    detail += ", ";
                detail += dependencies[index];
                if (detail.size() > 1'900u)
                {
                    detail.resize(1'896u);
                    detail += "...";
                    return;
                }
            }
        }

        [[nodiscard]] std::string system_detail(
            const systems::SystemDiagnostic& system)
        {
            std::string detail = "Order ";
            detail += std::to_string(system.execution_order);
            detail += " | Updates ";
            detail += std::to_string(system.update_count);
            detail += " | Last ";
            detail += duration_text(system.last_update_nanoseconds);
            detail += " | Peak ";
            detail += duration_text(system.peak_update_nanoseconds);
            detail += " | Mean ";
            detail += duration_text(system.update_count == 0u
                ? 0u
                : system.total_update_nanoseconds / system.update_count);
            append_dependencies(detail, system.dependencies);
            return detail;
        }

        [[nodiscard]] std::string task_row_id(graph::NodeHandle handle)
        {
            return "learning:"
                + std::to_string(handle.index)
                + ":"
                + std::to_string(handle.generation);
        }

        [[nodiscard]] std::vector<workspace::Row> system_rows(
            const systems::RegistryDiagnosticSnapshot& snapshot)
        {
            std::vector<workspace::Row> rows{};
            rows.reserve(snapshot.systems.size());
            for (const systems::SystemDiagnostic& system : snapshot.systems)
            {
                workspace::Row row{};
                row.id = "system:" + system.name;
                row.label = system.name;
                row.category = "Live Systems";
                row.detail = system_detail(system);
                row.kind = workspace::RowKind::system;
                row.status = system_status(snapshot, system);
                row.adapter_order = system.execution_order;
                row.revision = snapshot.revision;
                rows.push_back(std::move(row));
            }
            return rows;
        }

        [[nodiscard]] std::vector<workspace::Row> learning_rows(
            const graph::GraphDiagnostics& diagnostics)
        {
            std::vector<workspace::Row> rows{};
            rows.reserve(diagnostics.nodes().size());
            for (const graph::NodeDiagnostic& node : diagnostics.nodes())
            {
                workspace::Row row{};
                row.id = task_row_id(node.handle);
                row.label = node.name;
                row.category = "Learning Sandbox: " + node.category;
                row.detail = "Estimated work ";
                row.detail += std::to_string(node.estimated_cost_units);
                row.detail += " | Wave ";
                row.detail += std::to_string(node.parallel_wave);
                row.detail += " | Prerequisites ";
                row.detail += std::to_string(node.prerequisite_count);
                row.detail += " | Dependents ";
                row.detail += std::to_string(node.dependent_count);
                if (node.on_critical_path)
                    row.detail += " | Critical path";
                row.kind = workspace::RowKind::task;
                row.status = diagnostics.healthy()
                    ? workspace::RowStatus::idle
                    : workspace::RowStatus::failed;
                row.adapter_order = 1'000'000u
                    + static_cast<std::uint64_t>(node.parallel_wave) * 10'000u
                    + node.handle.index;
                row.revision = diagnostics.revision().sequence;
                rows.push_back(std::move(row));
            }
            return rows;
        }

        [[nodiscard]] constexpr std::string_view scheduler_state_name(
            taskgraph::NodeState state) noexcept
        {
            switch (state)
            {
            case taskgraph::NodeState::pending: return "Pending";
            case taskgraph::NodeState::queued: return "Queued";
            case taskgraph::NodeState::running: return "Running";
            case taskgraph::NodeState::completed: return "Completed";
            case taskgraph::NodeState::failed: return "Failed";
            case taskgraph::NodeState::cancelled: return "Cancelled";
            }
            return "Unknown";
        }

        [[nodiscard]] workspace::RowStatus scheduler_row_status(
            taskgraph::NodeState state) noexcept
        {
            switch (state)
            {
            case taskgraph::NodeState::pending: return workspace::RowStatus::waiting;
            case taskgraph::NodeState::queued: return workspace::RowStatus::queued;
            case taskgraph::NodeState::running: return workspace::RowStatus::running;
            case taskgraph::NodeState::completed: return workspace::RowStatus::healthy;
            case taskgraph::NodeState::failed: return workspace::RowStatus::failed;
            case taskgraph::NodeState::cancelled: return workspace::RowStatus::cancelled;
            }
            return workspace::RowStatus::unknown;
        }

        [[nodiscard]] std::vector<workspace::Row> live_scheduler_rows(
            const LiveSchedulerSnapshot& scheduler)
        {
            std::vector<workspace::Row> rows{};
            if (scheduler.state != LiveDataState::available)
                return rows;

            rows.reserve(scheduler.graph.nodes.size());
            for (const taskgraph::NodeDiagnostic& node : scheduler.graph.nodes)
            {
                workspace::Row row{};
                row.id = "scheduler:" + std::to_string(node.id);
                row.label = node.label.empty()
                    ? "Task " + std::to_string(node.id)
                    : node.label;
                row.category = "Live Scheduler (Read Only)";
                row.detail = "State ";
                row.detail += scheduler_state_name(node.state);
                row.detail += " | Remaining prerequisites ";
                row.detail += std::to_string(node.remainingPrerequisites);
                row.detail += " | Dependents ";
                row.detail += std::to_string(node.dependentCount);
                if (node.wasStarted)
                {
                    row.detail += " | Queue wait ";
                    row.detail += duration_text(node.queueLatencyNanoseconds);
                }
                if (node.wasFinished)
                {
                    row.detail += " | Run ";
                    row.detail += duration_text(node.runDurationNanoseconds);
                }
                row.kind = workspace::RowKind::task;
                row.status = scheduler_row_status(node.state);
                row.adapter_order = node.id;
                row.revision = scheduler.graph.revision;
                rows.push_back(std::move(row));
            }
            return rows;
        }

        [[nodiscard]] std::vector<workspace::Row> rows_for_tab(
            Tab tab,
            const systems::RegistryDiagnosticSnapshot& registry,
            const LiveSchedulerSnapshot& scheduler,
            const graph::GraphDiagnostics& learning)
        {
            if (tab == Tab::overview || tab == Tab::renderer || tab == Tab::time)
                return {};
            if (tab == Tab::systems)
                return system_rows(registry);
            if (tab == Tab::live_scheduler)
                return live_scheduler_rows(scheduler);
            if (tab == Tab::learning_graph)
                return learning_rows(learning);

            return {};

        }

        [[nodiscard]] ControllerResult graph_failure(
            graph::ResultCode code) noexcept
        {
            return ControllerResult{
                ControllerCode::graph_rejected,
                code,
                workspace::ErrorCode::none,
                false};
        }
    }

    struct SystemsWorkspace::Implementation final
    {
        ControllerLimits limits{};
        workspace::Controller workspace_controller;
        systems::RegistryDiagnosticSnapshot registry{};
        LiveSchedulerSnapshot live_scheduler{};
        TimeDiagnosticsSnapshot time_diagnostics{};
        graph::TaskGraphDocument learning_graph;
        Tab active_tab{Tab::overview};
        bool diagnostics_sampling{};
        bool initialized{};
        std::uint64_t source_revision{};
        std::uint64_t next_time_sequence{1u};
        std::uint64_t selected_scheduler_node{};
        std::uint64_t selected_time_sequence{};
        std::string status_message{};

        [[nodiscard]] static bool populate_default_learning_graph(
            graph::TaskGraphDocument& target)
        {
            const graph::NodeHandle request = target.add_node({
                "Authoring Request", "Input", 1u}).node;
            const graph::NodeHandle validate = target.add_node({
                "Validate Project", "Validation", 2u}).node;
            const graph::NodeHandle scripts = target.add_node({
                "Compile Scripts", "Build", 3u}).node;
            const graph::NodeHandle assets = target.add_node({
                "Compile Assets", "Build", 5u}).node;
            const graph::NodeHandle runtime = target.add_node({
                "Build Runtime", "Build", 4u}).node;
            const graph::NodeHandle run = target.add_node({
                "Run External", "Execution", 1u}).node;

            return request && validate && scripts && assets
                && runtime && run
                && target.connect(request, validate)
                && target.connect(validate, scripts)
                && target.connect(validate, assets)
                && target.connect(scripts, runtime)
                && target.connect(assets, runtime)
                && target.connect(runtime, run);
        }

        explicit Implementation(ControllerLimits requestedLimits)
            : limits(requestedLimits)
            , workspace_controller(workspace_limits(requestedLimits))
            , learning_graph(
                graph::DocumentHandle{1u, 1u},
                graph::BranchIdentity{1u, 1u},
                graph_limits(requestedLimits))
        {
            time_diagnostics.capacity = limits.maximum_time_samples;
            initialized = limits.valid() && learning_graph.valid();
            if (!initialized)
            {
                status_message = "Systems workspace configuration is invalid.";
                return;
            }

            const bool sampleReady =
                populate_default_learning_graph(learning_graph);
            if (!sampleReady)
            {
                initialized = false;
                status_message = "Learning sandbox initialization failed.";
                return;
            }

            status_message = "Learning sandbox is separate from the live scheduler.";
            const ControllerResult rebuilt = rebuild();
            initialized = static_cast<bool>(rebuilt);
        }

        [[nodiscard]] ControllerResult rebuild()
        {
            if (!initialized)
            {
                return ControllerResult{
                    ControllerCode::invalid_controller,
                    graph::ResultCode::invalid_document,
                    workspace::ErrorCode::invalid_limits,
                    false};
            }

            const graph::GraphDiagnostics diagnostics =
                learning_graph.diagnostics();
            std::vector<workspace::Row> rows = rows_for_tab(
                active_tab, registry, live_scheduler, diagnostics);
            ++source_revision;
            if (source_revision == 0u)
                source_revision = 1u;
            const workspace::RefreshResult refreshed =
                workspace_controller.replace_rows(rows, source_revision);
            if (!refreshed.committed)
            {
                status_message = "Reusable systems workspace rejected the model.";
                return ControllerResult{
                    ControllerCode::workspace_rejected,
                    graph::ResultCode::success,
                    refreshed.error,
                    false};
            }
            return ControllerResult{
                ControllerCode::ready,
                graph::ResultCode::success,
                workspace::ErrorCode::none,
                true};
        }

        [[nodiscard]] bool capture_time_sample(
            const systems::RegistryDiagnosticSnapshot& snapshot)
        {
            if (!diagnostics_sampling)
            {
                time_diagnostics.state = LiveDataState::paused;
                time_diagnostics.status =
                    "Timing history is paused while Systems is not visible.";
                return false;
            }
            if (!snapshot.diagnostics_sampling || snapshot.frame_count == 0u)
            {
                time_diagnostics.state = LiveDataState::warming_up;
                time_diagnostics.status =
                    "Registry timing is warming up; no measured frame is available yet.";
                return false;
            }
            if (!time_diagnostics.samples.empty())
            {
                const TimeSample& latest = time_diagnostics.samples.back();
                if (latest.registry_revision == snapshot.revision
                    && latest.frame_count == snapshot.frame_count)
                {
                    return false;
                }
            }

            TimeSample sample{};
            sample.sequence = next_time_sequence++;
            if (next_time_sequence == 0u)
                next_time_sequence = 1u;
            sample.registry_revision = snapshot.revision;
            sample.frame_count = snapshot.frame_count;
            sample.frame_nanoseconds = snapshot.last_frame_nanoseconds;
            sample.peak_frame_nanoseconds = snapshot.peak_frame_nanoseconds;
            for (const systems::SystemDiagnostic& system : snapshot.systems)
            {
                const std::uint64_t remaining =
                    (std::numeric_limits<std::uint64_t>::max)()
                    - sample.systems_nanoseconds;
                sample.systems_nanoseconds += (std::min)(
                    remaining, system.last_update_nanoseconds);
                if (system.last_update_nanoseconds
                    > sample.busiest_system_nanoseconds)
                {
                    sample.busiest_system_nanoseconds =
                        system.last_update_nanoseconds;
                    sample.busiest_system = system.name;
                }
            }

            if (time_diagnostics.samples.size() >= time_diagnostics.capacity)
            {
                if (selected_time_sequence
                    == time_diagnostics.samples.front().sequence)
                {
                    selected_time_sequence = 0u;
                }
                time_diagnostics.samples.pop_front();
                ++time_diagnostics.dropped_samples;
            }
            time_diagnostics.samples.push_back(std::move(sample));
            time_diagnostics.state = LiveDataState::available;
            time_diagnostics.status =
                "Measured registry frames sampled while Systems is visible.";
            ++time_diagnostics.revision;
            if (time_diagnostics.revision == 0u)
                time_diagnostics.revision = 1u;
            return true;
        }

        [[nodiscard]] LearningOperationResult accept_graph_mutation(
            graph::MutationResult mutation)
        {
            if (!mutation)
            {
                return LearningOperationResult{
                    graph_failure(mutation.code),
                    mutation.node,
                    mutation.operation};
            }
            ControllerResult rebuilt = rebuild();
            if (rebuilt)
                status_message = "Learning sandbox operation committed.";
            return LearningOperationResult{
                rebuilt,
                mutation.node,
                mutation.operation};
        }
    };

    SystemsWorkspace::SystemsWorkspace(ControllerLimits limits)
    {
        try
        {
            implementation_ = std::make_unique<Implementation>(limits);
        }
        catch (...)
        {
            implementation_.reset();
        }
    }

    SystemsWorkspace::~SystemsWorkspace() = default;
    SystemsWorkspace::SystemsWorkspace(SystemsWorkspace&&) noexcept = default;
    SystemsWorkspace& SystemsWorkspace::operator=(SystemsWorkspace&&) noexcept = default;

    bool SystemsWorkspace::valid() const noexcept
    {
        return implementation_ && implementation_->initialized;
    }

    const ControllerLimits& SystemsWorkspace::limits() const noexcept
    {
        static constexpr ControllerLimits invalid{};
        return implementation_ ? implementation_->limits : invalid;
    }

    Tab SystemsWorkspace::tab() const noexcept
    {
        return implementation_ ? implementation_->active_tab : Tab::overview;
    }

    ControllerResult SystemsWorkspace::set_tab(Tab tab)
    {
        if (!valid())
            return {ControllerCode::invalid_controller};
        if (implementation_->active_tab == tab)
            return {ControllerCode::unchanged};
        implementation_->active_tab = tab;
        ControllerResult result = implementation_->rebuild();
        if (result)
        {
            implementation_->status_message = tab == Tab::renderer
                ? "Renderer diagnostics remain owned by the renderer host."
                : tab == Tab::live_scheduler
                    ? implementation_->live_scheduler.status
                    : tab == Tab::time
                        ? implementation_->time_diagnostics.status
                        : tab == Tab::learning_graph
                            ? "Learning graph is isolated from the live scheduler."
                            : "Systems workspace tab changed.";
        }
        return result;
    }

    std::string_view SystemsWorkspace::text_filter() const noexcept
    {
        return valid()
            ? implementation_->workspace_controller.text_filter()
            : std::string_view{};
    }

    ControllerResult SystemsWorkspace::set_text_filter(std::string_view filter)
    {
        if (!valid())
            return {ControllerCode::invalid_controller};
        const workspace::MutationResult mutation =
            implementation_->workspace_controller.set_text_filter(filter);
        if (!mutation.accepted)
        {
            return {
                ControllerCode::workspace_rejected,
                graph::ResultCode::success,
                mutation.error,
                false};
        }
        return {
            mutation.changed ? ControllerCode::ready : ControllerCode::unchanged,
            graph::ResultCode::success,
            workspace::ErrorCode::none,
            mutation.changed};
    }

    ControllerResult SystemsWorkspace::clear_filters()
    {
        if (!valid())
            return {ControllerCode::invalid_controller};
        const workspace::MutationResult mutation =
            implementation_->workspace_controller.clear_filters();
        if (!mutation.accepted)
        {
            return {
                ControllerCode::workspace_rejected,
                graph::ResultCode::success,
                mutation.error,
                false};
        }
        return {
            mutation.changed ? ControllerCode::ready : ControllerCode::unchanged,
            graph::ResultCode::success,
            workspace::ErrorCode::none,
            mutation.changed};
    }

    std::string_view SystemsWorkspace::selected_id() const noexcept
    {
        return valid()
            ? implementation_->workspace_controller.selected_id()
            : std::string_view{};
    }

    ControllerResult SystemsWorkspace::select(std::string_view rowId)
    {
        if (!valid())
            return {ControllerCode::invalid_controller};
        const workspace::MutationResult mutation =
            implementation_->workspace_controller.select(rowId);
        if (!mutation.accepted)
        {
            return {
                ControllerCode::workspace_rejected,
                graph::ResultCode::success,
                mutation.error,
                false};
        }
        return {
            mutation.changed ? ControllerCode::ready : ControllerCode::unchanged,
            graph::ResultCode::success,
            workspace::ErrorCode::none,
            mutation.changed};
    }

    bool SystemsWorkspace::clear_selection() noexcept
    {
        if (!valid())
            return false;
        const bool workspaceChanged =
            implementation_->workspace_controller.clear_selection();
        const bool changed = workspaceChanged
            || implementation_->selected_scheduler_node != 0u
            || implementation_->selected_time_sequence != 0u;
        implementation_->selected_scheduler_node = 0u;
        implementation_->selected_time_sequence = 0u;
        return changed;
    }

    bool SystemsWorkspace::diagnostics_sampling_intent() const noexcept
    {
        return valid() && implementation_->diagnostics_sampling;
    }

    ControllerResult SystemsWorkspace::set_diagnostics_sampling_intent(
        bool enabled) noexcept
    {
        if (!valid())
            return {ControllerCode::invalid_controller};
        if (implementation_->diagnostics_sampling == enabled)
            return {ControllerCode::unchanged};
        implementation_->diagnostics_sampling = enabled;
        implementation_->time_diagnostics.state = enabled
            ? LiveDataState::warming_up
            : LiveDataState::paused;
        implementation_->time_diagnostics.status = enabled
            ? "Timing diagnostics are waiting for the next measured registry frame."
            : "Timing history is paused while Systems is not visible.";
        implementation_->status_message = enabled
            ? "Live timing sampling requested by the visible Systems workspace."
            : "Live timing sampling is not requested.";
        return {
            ControllerCode::ready,
            graph::ResultCode::success,
            workspace::ErrorCode::none,
            true};
    }

    RefreshResult SystemsWorkspace::refresh_from_registry(
        systems::Registry& registry)
    {
        return refresh_from_snapshot(registry.diagnostics());
    }

    RefreshResult SystemsWorkspace::refresh_from_snapshot(
        systems::RegistryDiagnosticSnapshot snapshot)
    {
        RefreshResult result{};
        if (!valid())
        {
            result.result.code = ControllerCode::invalid_controller;
            return result;
        }
        result.registry_systems = snapshot.systems.size();
        result.registry_revision = snapshot.revision;
        if (snapshot.systems.size()
            > implementation_->limits.maximum_registry_systems)
        {
            result.result.code = ControllerCode::registry_limit_exceeded;
            return result;
        }
        const std::size_t maximumName = implementation_->limits.maximum_name_bytes;
        if (snapshot.last_error.size() > maximumName * 8u)
        {
            result.result.code = ControllerCode::workspace_rejected;
            return result;
        }
        for (const systems::SystemDiagnostic& system : snapshot.systems)
        {
            if (system.name.empty() || system.name.size() > maximumName
                || system.dependencies.size()
                    > implementation_->limits.maximum_registry_systems)
            {
                result.result.code = ControllerCode::workspace_rejected;
                return result;
            }
            const bool invalidDependency = std::any_of(
                system.dependencies.begin(), system.dependencies.end(),
                [maximumName](const std::string& dependency)
                {
                    return dependency.empty() || dependency.size() > maximumName;
                });
            if (invalidDependency)
            {
                result.result.code = ControllerCode::workspace_rejected;
                return result;
            }
        }

        auto& current = implementation_->registry;
        const bool registryChanged = current.revision != snapshot.revision
            || current.diagnostics_sampling != snapshot.diagnostics_sampling
            || current.order_resolved != snapshot.order_resolved
            || current.initialized != snapshot.initialized
            || current.last_error != snapshot.last_error
            || current.systems.size() != snapshot.systems.size();
        if (!registryChanged)
        {
            const bool sampledTime = implementation_->capture_time_sample(snapshot);
            current.frame_count = snapshot.frame_count;
            current.last_frame_nanoseconds = snapshot.last_frame_nanoseconds;
            current.peak_frame_nanoseconds = snapshot.peak_frame_nanoseconds;
            for (std::size_t index = 0u; index < current.systems.size(); ++index)
            {
                current.systems[index].update_count =
                    snapshot.systems[index].update_count;
                current.systems[index].last_update_nanoseconds =
                    snapshot.systems[index].last_update_nanoseconds;
                current.systems[index].peak_update_nanoseconds =
                    snapshot.systems[index].peak_update_nanoseconds;
                current.systems[index].total_update_nanoseconds =
                    snapshot.systems[index].total_update_nanoseconds;
            }
            result.result = {
                sampledTime ? ControllerCode::ready : ControllerCode::unchanged,
                graph::ResultCode::success,
                workspace::ErrorCode::none,
                sampledTime};
            result.registry_systems = current.systems.size();
            result.registry_revision = current.revision;
            result.committed_rows =
                implementation_->workspace_controller.rows().size();
            result.visible_rows =
                implementation_->workspace_controller.visible_rows().size();
            result.view_revision =
                implementation_->workspace_controller.view_revision();
            return result;
        }

        systems::RegistryDiagnosticSnapshot previous =
            std::move(implementation_->registry);
        implementation_->registry = std::move(snapshot);
        const bool registryProjection =
            implementation_->active_tab == Tab::systems;
        ControllerResult rebuilt = registryProjection
            ? implementation_->rebuild()
            : ControllerResult{
                ControllerCode::ready,
                graph::ResultCode::success,
                workspace::ErrorCode::none,
                true};
        if (!rebuilt)
        {
            implementation_->registry = std::move(previous);
            if (registryProjection)
                (void)implementation_->rebuild();
            result.result = rebuilt;
            return result;
        }

        const bool sampledTime = implementation_->capture_time_sample(
            implementation_->registry);
        implementation_->status_message = implementation_->registry.last_error.empty()
            ? sampledTime
                ? "Live systems diagnostics and bounded timing history refreshed."
                : "Live systems diagnostics refreshed."
            : "Live registry reported an error; owning diagnostics were preserved.";
        result.result = rebuilt;
        result.registry_systems = implementation_->registry.systems.size();
        result.registry_revision = implementation_->registry.revision;
        result.committed_rows =
            implementation_->workspace_controller.rows().size();
        result.visible_rows =
            implementation_->workspace_controller.visible_rows().size();
        result.view_revision =
            implementation_->workspace_controller.view_revision();
        return result;
    }
    SchedulerRefreshResult SystemsWorkspace::refresh_live_scheduler(
        const taskgraph::TaskGraph* scheduler,
        std::string_view ownerLabel)
    {
        if (!scheduler)
        {
            return mark_live_scheduler_unavailable(
                "No live scheduler owner is attached to the Systems panel.");
        }
        try
        {
            std::string resolvedOwner = ownerLabel.empty()
                ? std::string{"Attached TaskGraph"}
                : std::string{ownerLabel};
            if (valid())
            {
                const auto& current = implementation_->live_scheduler;
                const std::uint64_t revision = scheduler->Revision();
                if (current.state == LiveDataState::available
                    && current.owner_label == resolvedOwner
                    && current.graph.revision == revision)
                {
                    return SchedulerRefreshResult{
                        .result = ControllerResult{ControllerCode::unchanged},
                        .state = current.state,
                        .visible_nodes = current.graph.nodes.size(),
                        .total_nodes = current.graph.nodeCount,
                        .scheduler_revision = current.graph.revision,
                        .view_revision = current.view_revision};
                }
            }
            return refresh_live_scheduler_snapshot(
                scheduler->Snapshot(
                    implementation_
                        ? implementation_->limits.maximum_live_scheduler_nodes
                        : 0u),
                std::move(resolvedOwner));
        }
        catch (...)
        {
            return mark_live_scheduler_unavailable(
                "The attached scheduler snapshot could not be captured.");
        }
    }

    SchedulerRefreshResult SystemsWorkspace::refresh_live_scheduler_snapshot(
        taskgraph::GraphSnapshot snapshot,
        std::string ownerLabel)
    {
        SchedulerRefreshResult result{};
        if (!valid())
        {
            result.result.code = ControllerCode::invalid_controller;
            return result;
        }
        result.visible_nodes = snapshot.nodes.size();
        result.total_nodes = snapshot.nodeCount;
        result.scheduler_revision = snapshot.revision;
        if (snapshot.revision == 0u
            || snapshot.nodes.size()
                > implementation_->limits.maximum_live_scheduler_nodes
            || snapshot.nodeCount < snapshot.nodes.size())
        {
            result.result.code = ControllerCode::scheduler_limit_exceeded;
            return result;
        }
        for (std::size_t index = 0u; index < snapshot.nodes.size(); ++index)
        {
            const taskgraph::NodeDiagnostic& node = snapshot.nodes[index];
            const bool hasTiming = node.wasQueued
                || node.wasStarted || node.wasFinished;
            const bool invalidTiming =
                (node.wasStarted && !node.wasQueued)
                || (hasTiming && node.acceptedNanoseconds == 0u)
                || (node.wasQueued
                    && node.queuedNanoseconds < node.acceptedNanoseconds)
                || (node.wasStarted
                    && node.startedNanoseconds < node.queuedNanoseconds)
                || (node.wasFinished && node.wasStarted
                    && node.finishedNanoseconds < node.startedNanoseconds)
                || (node.wasFinished && !node.wasStarted
                    && node.finishedNanoseconds < node.acceptedNanoseconds)
                || (node.wasStarted
                    && node.queueLatencyNanoseconds
                        != node.startedNanoseconds - node.queuedNanoseconds)
                || (node.wasFinished && node.wasStarted
                    && node.runDurationNanoseconds
                        != node.finishedNanoseconds - node.startedNanoseconds);
            if (node.id == 0u
                || node.label.size()
                    > implementation_->limits.maximum_name_bytes
                || invalidTiming)
            {
                result.result.code = ControllerCode::workspace_rejected;
                return result;
            }
            const auto duplicate = std::find_if(
                snapshot.nodes.begin() + static_cast<std::ptrdiff_t>(index + 1u),
                snapshot.nodes.end(),
                [&snapshot, index](const taskgraph::NodeDiagnostic& candidate)
                {
                    return candidate.id == snapshot.nodes[index].id;
                });
            if (duplicate != snapshot.nodes.end())
            {
                result.result.code = ControllerCode::workspace_rejected;
                return result;
            }
        }

        if (ownerLabel.empty())
            ownerLabel = "Attached TaskGraph";
        if (ownerLabel.size() > implementation_->limits.maximum_name_bytes)
        {
            result.result.code = ControllerCode::workspace_rejected;
            return result;
        }
        const auto& current = implementation_->live_scheduler;
        if (current.state == LiveDataState::available
            && current.owner_label == ownerLabel
            && current.graph.revision == snapshot.revision)
        {
            result.result = {ControllerCode::unchanged};
            result.state = current.state;
            result.visible_nodes = current.graph.nodes.size();
            result.total_nodes = current.graph.nodeCount;
            result.scheduler_revision = current.graph.revision;
            result.view_revision = current.view_revision;
            return result;
        }

        LiveSchedulerSnapshot next{};
        next.state = LiveDataState::available;
        next.owner_label = std::move(ownerLabel);
        next.status =
            "Read-only owning snapshot captured from the attached live scheduler.";
        next.graph = std::move(snapshot);
        next.view_revision = current.view_revision + 1u;
        if (next.view_revision == 0u)
            next.view_revision = 1u;
        implementation_->live_scheduler = std::move(next);
        if (implementation_->selected_scheduler_node != 0u
            && !selected_live_scheduler_node())
        {
            implementation_->selected_scheduler_node = 0u;
        }

        ControllerResult rebuilt =
            implementation_->active_tab == Tab::live_scheduler
            ? implementation_->rebuild()
            : ControllerResult{
                ControllerCode::ready,
                graph::ResultCode::success,
                workspace::ErrorCode::none,
                true};
        implementation_->status_message = implementation_->live_scheduler.status;
        result.result = rebuilt;
        result.state = implementation_->live_scheduler.state;
        result.visible_nodes = implementation_->live_scheduler.graph.nodes.size();
        result.total_nodes = implementation_->live_scheduler.graph.nodeCount;
        result.scheduler_revision = implementation_->live_scheduler.graph.revision;
        result.view_revision = implementation_->live_scheduler.view_revision;
        return result;
    }

    SchedulerRefreshResult SystemsWorkspace::mark_live_scheduler_unavailable(
        std::string reason)
    {
        SchedulerRefreshResult result{};
        if (!valid())
        {
            result.result.code = ControllerCode::invalid_controller;
            return result;
        }
        if (reason.empty())
            reason = "No live scheduler owner is attached to the Systems panel.";
        const std::size_t maximumReason =
            static_cast<std::size_t>(implementation_->limits.maximum_name_bytes) * 4u;
        if (reason.size() > maximumReason)
            reason.resize(maximumReason);
        auto& current = implementation_->live_scheduler;
        if (current.state == LiveDataState::unavailable
            && current.status == reason)
        {
            result.result = {ControllerCode::unchanged};
            result.state = current.state;
            result.view_revision = current.view_revision;
            return result;
        }

        const std::uint64_t nextRevision = current.view_revision + 1u;
        current = LiveSchedulerSnapshot{};
        current.state = LiveDataState::unavailable;
        current.status = std::move(reason);
        current.view_revision = nextRevision == 0u ? 1u : nextRevision;
        implementation_->selected_scheduler_node = 0u;
        ControllerResult rebuilt =
            implementation_->active_tab == Tab::live_scheduler
            ? implementation_->rebuild()
            : ControllerResult{
                ControllerCode::ready,
                graph::ResultCode::success,
                workspace::ErrorCode::none,
                true};
        implementation_->status_message = current.status;
        result.result = rebuilt;
        result.state = current.state;
        result.view_revision = current.view_revision;
        return result;
    }
    ControllerResult SystemsWorkspace::reset()
    {
        if (!implementation_)
            return {ControllerCode::invalid_controller};
        const ControllerLimits retained = implementation_->limits;
        try
        {
            auto replacement = std::make_unique<Implementation>(retained);
            if (!replacement->initialized)
                return {ControllerCode::invalid_controller};
            implementation_ = std::move(replacement);
            return {
                ControllerCode::ready,
                graph::ResultCode::success,
                workspace::ErrorCode::none,
                true};
        }
        catch (...)
        {
            return {ControllerCode::allocation_failure};
        }
    }

    ControllerResult SystemsWorkspace::reset_learning_graph()
    {
        if (!implementation_)
            return {ControllerCode::invalid_controller};
        try
        {
            graph::TaskGraphDocument replacement{
                graph::DocumentHandle{1u, 1u},
                graph::BranchIdentity{1u, 1u},
                graph_limits(implementation_->limits)};
            if (!replacement.valid()
                || !Implementation::populate_default_learning_graph(
                    replacement))
            {
                return {
                    ControllerCode::graph_rejected,
                    graph::ResultCode::invalid_document,
                    workspace::ErrorCode::none,
                    false};
            }

            implementation_->learning_graph = std::move(replacement);
            ControllerResult rebuilt =
                implementation_->active_tab == Tab::learning_graph
                ? implementation_->rebuild()
                : ControllerResult{
                    ControllerCode::ready,
                    graph::ResultCode::success,
                    workspace::ErrorCode::none,
                    true};
            if (rebuilt)
            {
                implementation_->status_message =
                    "Learning graph restored to its default document.";
            }
            return rebuilt;
        }
        catch (...)
        {
            return {ControllerCode::allocation_failure};
        }
    }

    const systems::RegistryDiagnosticSnapshot&
        SystemsWorkspace::registry_snapshot() const noexcept
    {
        static const systems::RegistryDiagnosticSnapshot empty{};
        return valid() ? implementation_->registry : empty;
    }

    const LiveSchedulerSnapshot&
        SystemsWorkspace::live_scheduler_snapshot() const noexcept
    {
        static const LiveSchedulerSnapshot empty{};
        return valid() ? implementation_->live_scheduler : empty;
    }

    ControllerResult SystemsWorkspace::select_live_scheduler_node(
        std::uint64_t nodeId) noexcept
    {
        if (!valid())
            return {ControllerCode::invalid_controller};
        const auto& nodes = implementation_->live_scheduler.graph.nodes;
        const auto found = std::find_if(
            nodes.begin(), nodes.end(), [nodeId](const auto& node)
            {
                return node.id == nodeId;
            });
        if (found == nodes.end())
            return {ControllerCode::workspace_rejected};
        if (implementation_->selected_scheduler_node == nodeId)
            return {ControllerCode::unchanged};
        implementation_->selected_scheduler_node = nodeId;
        return {
            ControllerCode::ready,
            graph::ResultCode::success,
            workspace::ErrorCode::none,
            true};
    }

    const taskgraph::NodeDiagnostic*
        SystemsWorkspace::selected_live_scheduler_node() const noexcept
    {
        if (!valid() || implementation_->selected_scheduler_node == 0u)
            return nullptr;
        const auto& nodes = implementation_->live_scheduler.graph.nodes;
        const auto found = std::find_if(
            nodes.begin(), nodes.end(), [this](const auto& node)
            {
                return node.id == implementation_->selected_scheduler_node;
            });
        return found == nodes.end() ? nullptr : &*found;
    }

    const TimeDiagnosticsSnapshot&
        SystemsWorkspace::time_diagnostics() const noexcept
    {
        static const TimeDiagnosticsSnapshot empty{};
        return valid() ? implementation_->time_diagnostics : empty;
    }

    ControllerResult SystemsWorkspace::select_time_sample(
        std::uint64_t sequence) noexcept
    {
        if (!valid())
            return {ControllerCode::invalid_controller};
        const auto& samples = implementation_->time_diagnostics.samples;
        const auto found = std::find_if(
            samples.begin(), samples.end(), [sequence](const TimeSample& sample)
            {
                return sample.sequence == sequence;
            });
        if (found == samples.end())
            return {ControllerCode::workspace_rejected};
        if (implementation_->selected_time_sequence == sequence)
            return {ControllerCode::unchanged};
        implementation_->selected_time_sequence = sequence;
        return {
            ControllerCode::ready,
            graph::ResultCode::success,
            workspace::ErrorCode::none,
            true};
    }

    const TimeSample* SystemsWorkspace::selected_time_sample() const noexcept
    {
        if (!valid() || implementation_->selected_time_sequence == 0u)
            return nullptr;
        const auto& samples = implementation_->time_diagnostics.samples;
        const auto found = std::find_if(
            samples.begin(), samples.end(), [this](const TimeSample& sample)
            {
                return sample.sequence == implementation_->selected_time_sequence;
            });
        return found == samples.end() ? nullptr : &*found;
    }

    const workspace::Controller& SystemsWorkspace::workspace() const noexcept
    {
        static const workspace::Controller empty{};
        return valid() ? implementation_->workspace_controller : empty;
    }

    LearningOperationResult SystemsWorkspace::add_learning_task(
        std::string name,
        std::string category,
        std::uint64_t estimatedCostUnits)
    {
        if (!valid())
            return {{ControllerCode::invalid_controller}, {}, {}};
        return implementation_->accept_graph_mutation(
            implementation_->learning_graph.add_node({
                std::move(name),
                std::move(category),
                estimatedCostUnits}));
    }

    LearningOperationResult SystemsWorkspace::connect_learning_tasks(
        graph::NodeHandle prerequisite,
        graph::NodeHandle dependent)
    {
        if (!valid())
            return {{ControllerCode::invalid_controller}, {}, {}};
        return implementation_->accept_graph_mutation(
            implementation_->learning_graph.connect(prerequisite, dependent));
    }

    LearningOperationResult SystemsWorkspace::disconnect_learning_tasks(
        graph::NodeHandle prerequisite,
        graph::NodeHandle dependent)
    {
        if (!valid())
            return {{ControllerCode::invalid_controller}, {}, {}};
        return implementation_->accept_graph_mutation(
            implementation_->learning_graph.disconnect(
                prerequisite,
                dependent));
    }

    LearningOperationResult SystemsWorkspace::rename_learning_task(
        graph::NodeHandle node,
        std::string name,
        std::string category,
        std::uint64_t costUnits)
    {
        if (!valid())
            return {{ControllerCode::invalid_controller}, {}, {}};
        return implementation_->accept_graph_mutation(
            implementation_->learning_graph.update_node(
                node,
                {
                    .name = std::move(name),
                    .category = std::move(category),
                    .estimated_cost_units = costUnits
                }));
    }
    LearningOperationResult SystemsWorkspace::remove_learning_task(
        graph::NodeHandle node)
    {
        if (!valid())
            return {{ControllerCode::invalid_controller}, {}, {}};
        return implementation_->accept_graph_mutation(
            implementation_->learning_graph.remove_node(node));
    }

    LearningOperationResult SystemsWorkspace::undo_learning_graph()
    {
        if (!valid())
            return {{ControllerCode::invalid_controller}, {}, {}};
        return implementation_->accept_graph_mutation(
            implementation_->learning_graph.undo());
    }

    LearningOperationResult SystemsWorkspace::redo_learning_graph()
    {
        if (!valid())
            return {{ControllerCode::invalid_controller}, {}, {}};
        return implementation_->accept_graph_mutation(
            implementation_->learning_graph.redo());
    }

    bool SystemsWorkspace::can_undo_learning_graph() const noexcept
    {
        return valid() && implementation_->learning_graph.can_undo();
    }

    bool SystemsWorkspace::can_redo_learning_graph() const noexcept
    {
        return valid() && implementation_->learning_graph.can_redo();
    }

    std::vector<graph::NodeView> SystemsWorkspace::learning_tasks() const
    {
        return valid()
            ? implementation_->learning_graph.nodes()
            : std::vector<graph::NodeView>{};
    }

    graph::GraphDiagnostics SystemsWorkspace::learning_diagnostics() const
    {
        return valid()
            ? implementation_->learning_graph.diagnostics()
            : graph::GraphDiagnostics{};
    }

    graph::ParallelWaveSimulation
        SystemsWorkspace::simulate_learning_graph() const
    {
        return valid()
            ? implementation_->learning_graph.simulate_parallel_waves()
            : graph::ParallelWaveSimulation{};
    }

    std::string_view SystemsWorkspace::status() const noexcept
    {
        return implementation_
            ? std::string_view{implementation_->status_message}
            : std::string_view{"Systems workspace allocation failed."};
    }
}
