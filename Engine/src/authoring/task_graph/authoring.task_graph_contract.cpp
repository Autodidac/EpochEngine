/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module authoring.task_graph;

namespace epochengine::authoring::task_graph
{
    namespace
    {
        [[nodiscard]] TaskGraphDocument make_document(
            GraphLimits limits = {})
        {
            return TaskGraphDocument{
                DocumentHandle{7u, 1u},
                BranchIdentity{11u, 3u},
                limits
            };
        }

        [[nodiscard]] NodeDescriptor node(
            std::string name,
            std::string category,
            std::uint64_t cost)
        {
            return NodeDescriptor{
                std::move(name),
                std::move(category),
                cost
            };
        }

        [[nodiscard]] bool same_handles(
            std::span<const NodeHandle> actual,
            std::initializer_list<NodeHandle> expected)
        {
            return std::equal(
                actual.begin(), actual.end(),
                expected.begin(), expected.end());
        }
    }

    ContractFailure run_contract()
    {
        TaskGraphDocument invalid{DocumentHandle{}, BranchIdentity{11u, 3u}};
        if (invalid.valid()
            || invalid.add_node(node("No", "Invalid", 1u)).code
                != ResultCode::invalid_document
            || invalid.topological_order().code
                != ResultCode::invalid_document)
        {
            return ContractFailure::invalid_document;
        }

        TaskGraphDocument descriptors = make_document();
        if (descriptors.add_node(node("", "Runtime", 1u)).code
                != ResultCode::empty_name
            || descriptors.add_node(node("Node", "", 1u)).code
                != ResultCode::empty_category
            || descriptors.add_node(node("Node", "Runtime", 0u)).code
                != ResultCode::invalid_estimated_cost
            || descriptors.add_node(node(
                    std::string(257u, 'n'), "Runtime", 1u)).code
                != ResultCode::name_too_long
            || descriptors.add_node(node(
                    "Node", std::string(129u, 'c'), 1u)).code
                != ResultCode::category_too_long
            || descriptors.add_node(node(
                    std::string{"bad\0name", 8u}, "Runtime", 1u)).code
                != ResultCode::invalid_text
            || !descriptors.nodes().empty())
        {
            return ContractFailure::descriptor_validation;
        }

        TaskGraphDocument graph = make_document();
        const NodeHandle load =
            graph.add_node(node("Load", "IO", 2u)).node;
        const NodeHandle cull =
            graph.add_node(node("Cull", "Render", 3u)).node;
        const NodeHandle physics =
            graph.add_node(node("Physics", "Simulation", 5u)).node;
        const NodeHandle render =
            graph.add_node(node("Render", "Render", 7u)).node;
        const NodeHandle present =
            graph.add_node(node("Present", "Platform", 1u)).node;
        if (!load || !cull || !physics || !render || !present
            || !graph.connect(load, cull)
            || !graph.connect(load, physics)
            || !graph.connect(cull, render)
            || !graph.connect(physics, render)
            || !graph.connect(render, present))
        {
            return ContractFailure::deterministic_order;
        }

        const TopologicalOrder firstOrder = graph.topological_order();
        const TopologicalOrder secondOrder = graph.topological_order();
        if (!firstOrder || firstOrder.nodes != secondOrder.nodes
            || !same_handles(
                firstOrder.nodes,
                {load, cull, physics, render, present}))
        {
            return ContractFailure::deterministic_order;
        }

        const CriticalPath critical = graph.critical_path();
        if (!critical
            || critical.estimated_cost_units != 15u
            || !same_handles(critical.nodes, {load, physics, render, present}))
        {
            return ContractFailure::critical_path;
        }

        const ParallelWaveSimulation waves = graph.simulate_parallel_waves();
        if (!waves
            || waves.waves.size() != 4u
            || waves.total_work_units != 18u
            || waves.barrier_elapsed_units != 15u
            || waves.maximum_parallel_width != 2u
            || !same_handles(waves.waves[0].nodes, {load})
            || !same_handles(waves.waves[1].nodes, {cull, physics})
            || !same_handles(waves.waves[2].nodes, {render})
            || !same_handles(waves.waves[3].nodes, {present})
            || waves.waves[1].total_work_units != 8u
            || waves.waves[1].barrier_cost_units != 5u)
        {
            return ContractFailure::parallel_waves;
        }

        const DocumentRevision beforeRejectedEdges = graph.revision();
        if (graph.connect(load, cull).code != ResultCode::duplicate_edge
            || graph.connect(load, load).code != ResultCode::self_edge
            || graph.connect(NodeHandle{}, load).code
                != ResultCode::invalid_handle
            || graph.connect(NodeHandle{900u, 1u}, load).code
                != ResultCode::missing_node
            || graph.disconnect(cull, physics).code
                != ResultCode::missing_edge
            || graph.revision() != beforeRejectedEdges)
        {
            return ContractFailure::edge_validation;
        }

        const GraphSnapshot beforeCycle = graph.snapshot();
        if (graph.connect(present, load).code != ResultCode::cycle_detected
            || graph.snapshot() != beforeCycle
            || graph.validate().code != ResultCode::success)
        {
            return ContractFailure::atomic_cycle_rejection;
        }

        const MutationResult removedPhysics = graph.remove_node(physics);
        if (!removedPhysics
            || graph.node(physics)
            || graph.rename_node(physics, "Stale").code
                != ResultCode::stale_handle
            || graph.edges().size() != 3u)
        {
            return ContractFailure::stale_identity;
        }
        if (!graph.undo()
            || !graph.node(physics)
            || graph.edges().size() != 5u
            || !graph.redo()
            || graph.node(physics)
            || graph.edges().size() != 3u
            || !graph.undo())
        {
            return ContractFailure::semantic_history;
        }

        const std::string oldName = graph.node(cull)->name;
        const DocumentRevision beforeRename = graph.revision();
        const MutationResult renamed = graph.rename_node(cull, "Visibility");
        if (!renamed
            || graph.node(cull)->name != "Visibility"
            || graph.revision() == beforeRename
            || !graph.undo()
            || graph.node(cull)->name != oldName
            || !graph.redo()
            || graph.node(cull)->name != "Visibility"
            || graph.rename_node(cull, "Visibility").code
                != ResultCode::no_change)
        {
            return ContractFailure::semantic_history;
        }

        TaskGraphDocument branch = make_document();
        const NodeHandle discarded =
            branch.add_node(node("Discarded", "Test", 1u)).node;
        if (!discarded || !branch.undo() || !branch.can_redo())
            return ContractFailure::history_branch_generation;
        const NodeHandle replacement =
            branch.add_node(node("Replacement", "Test", 1u)).node;
        if (!replacement
            || replacement.index != discarded.index
            || replacement.generation == discarded.generation
            || branch.can_redo()
            || branch.node(discarded)
            || branch.history().size() != 1u)
        {
            return ContractFailure::history_branch_generation;
        }

        const GraphSnapshot saved = graph.snapshot();
        if (!graph.disconnect(load, cull)
            || graph.snapshot() == saved
            || !graph.restore_snapshot(saved)
            || graph.snapshot().slots != saved.slots
            || graph.snapshot().edges != saved.edges
            || graph.revision().content != saved.revision.content
            || graph.can_undo()
            || graph.can_redo())
        {
            return ContractFailure::snapshot_restore;
        }

        GraphSnapshot wrongSchema = saved;
        ++wrongSchema.schema_version;
        GraphSnapshot wrongDocument = saved;
        wrongDocument.document = DocumentHandle{8u, 1u};
        GraphSnapshot corruptRevision = saved;
        ++corruptRevision.revision.content.words[0];
        GraphSnapshot duplicateEdge = saved;
        duplicateEdge.edges.push_back(duplicateEdge.edges.front());
        GraphSnapshot selfEdge = saved;
        selfEdge.edges.push_back(Edge{load, load});
        GraphSnapshot cyclic = saved;
        cyclic.edges.push_back(Edge{present, load});
        GraphSnapshot stale = saved;
        stale.edges.front().prerequisite.generation += 100u;
        const GraphSnapshot unchanged = graph.snapshot();
        if (graph.restore_snapshot(wrongSchema).code
                != ResultCode::invalid_snapshot_schema
            || graph.restore_snapshot(wrongDocument).code
                != ResultCode::invalid_snapshot_document
            || graph.restore_snapshot(corruptRevision).code
                != ResultCode::invalid_snapshot_revision
            || graph.restore_snapshot(duplicateEdge).code
                != ResultCode::malformed_snapshot
            || graph.restore_snapshot(selfEdge).code
                != ResultCode::malformed_snapshot
            || graph.restore_snapshot(cyclic).code
                != ResultCode::malformed_snapshot
            || graph.restore_snapshot(stale).code
                != ResultCode::malformed_snapshot
            || graph.snapshot() != unchanged)
        {
            return ContractFailure::snapshot_validation;
        }

        GraphLimits tiny{};
        tiny.maximum_active_nodes = 2u;
        tiny.maximum_node_slots = 2u;
        tiny.maximum_edges = 2u;
        tiny.maximum_history_operations = 8u;
        tiny.maximum_node_cost_units = 5u;
        tiny.maximum_total_cost_units = 6u;
        TaskGraphDocument bounded = make_document(tiny);
        if (bounded.add_node(node("Heavy", "Test", 6u)).code
            != ResultCode::invalid_estimated_cost)
        {
            return ContractFailure::bounded_state;
        }
        const NodeHandle one = bounded.add_node(node("One", "Test", 3u)).node;
        const NodeHandle two = bounded.add_node(node("Two", "Test", 3u)).node;
        if (!one || !two
            || bounded.add_node(node("Three", "Test", 1u)).code
                != ResultCode::node_limit_exceeded
            || !bounded.connect(one, two)
            || bounded.connect(two, one).code != ResultCode::cycle_detected)
        {
            return ContractFailure::bounded_state;
        }

        GraphLimits edgeBound = tiny;
        edgeBound.maximum_active_nodes = 3u;
        edgeBound.maximum_node_slots = 3u;
        edgeBound.maximum_edges = 1u;
        edgeBound.maximum_total_cost_units = 15u;
        TaskGraphDocument boundedEdges = make_document(edgeBound);
        const NodeHandle edgeA =
            boundedEdges.add_node(node("A", "Test", 1u)).node;
        const NodeHandle edgeB =
            boundedEdges.add_node(node("B", "Test", 1u)).node;
        const NodeHandle edgeC =
            boundedEdges.add_node(node("C", "Test", 1u)).node;
        if (!boundedEdges.connect(edgeA, edgeB)
            || boundedEdges.connect(edgeA, edgeC).code
                != ResultCode::edge_limit_exceeded)
        {
            return ContractFailure::bounded_state;
        }

        GraphLimits totalBound = edgeBound;
        totalBound.maximum_total_cost_units = 6u;
        TaskGraphDocument boundedTotal = make_document(totalBound);
        if (!boundedTotal.add_node(node("First", "Test", 3u))
            || !boundedTotal.add_node(node("Second", "Test", 3u))
            || boundedTotal.add_node(node("Overflow", "Test", 1u)).code
                != ResultCode::total_cost_limit_exceeded)
        {
            return ContractFailure::bounded_state;
        }

        GraphLimits historyBound{};
        historyBound.maximum_history_operations = 1u;
        TaskGraphDocument boundedHistory = make_document(historyBound);
        const NodeHandle only =
            boundedHistory.add_node(node("Only", "Test", 1u)).node;
        if (!only
            || boundedHistory.rename_node(only, "Rejected").code
                != ResultCode::history_limit_exceeded
            || boundedHistory.node(only)->name != "Only")
        {
            return ContractFailure::bounded_state;
        }

        GraphDiagnostics diagnostics = graph.diagnostics();
        if (!diagnostics.healthy()
            || diagnostics.revision() != graph.revision()
            || diagnostics.nodes().size() != graph.nodes().size()
            || diagnostics.edges().size() != graph.edges().size()
            || diagnostics.metrics().active_nodes != 5u
            || diagnostics.metrics().edges != 5u
            || diagnostics.metrics().critical_path_units != 15u
            || diagnostics.metrics().maximum_parallel_width != 2u)
        {
            return ContractFailure::immutable_diagnostics;
        }
        const std::string detachedName = diagnostics.nodes().front().name;
        if (!graph.rename_node(load, "Load Assets")
            || diagnostics.nodes().front().name != detachedName
            || diagnostics.revision() == graph.revision())
        {
            return ContractFailure::immutable_diagnostics;
        }

        const std::vector<OperationRecord> journal = graph.history();
        if (journal.empty()
            || journal.back().kind() != OperationKind::node_renamed
            || !journal.back().applied
            || !journal.back().id
            || operation_kind_name(journal.front().kind()).empty())
        {
            return ContractFailure::operation_journal;
        }

        if (result_code_name(ResultCode::cycle_detected) != "cycle_detected"
            || operation_kind_name(OperationKind::edge_disconnected)
                != "edge_disconnected")
        {
            return ContractFailure::result_names;
        }
        return ContractFailure::none;
    }
}

#if defined(EPOCH_AUTHORING_TASK_GRAPH_CONTRACT_MAIN)
int main()
{
    return static_cast<int>(
        epochengine::authoring::task_graph::run_contract());
}
#endif
