/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module authoring.task_graph;

import authoring.document;

export namespace epochengine::authoring::task_graph
{
    inline constexpr std::uint32_t kSchemaVersion = 1u;

    struct NodeTag final {};
    using NodeHandle = authoring::Handle<NodeTag, std::uint32_t>;
    using DocumentHandle = authoring::DocumentHandle;
    using DocumentRevision = authoring::DocumentRevision;
    using BranchIdentity = authoring::BranchIdentity;
    using OperationId = authoring::OperationId;

    struct NodeDescriptor final
    {
        std::string name{};
        std::string category{};
        std::uint64_t estimated_cost_units{1u};

        friend bool operator==(
            const NodeDescriptor&,
            const NodeDescriptor&) noexcept = default;
    };

    struct Edge final
    {
        NodeHandle prerequisite{};
        NodeHandle dependent{};

        friend constexpr auto operator<=>(
            const Edge&,
            const Edge&) noexcept = default;
    };

    struct GraphLimits final
    {
        std::uint32_t maximum_active_nodes{4'096u};
        std::uint32_t maximum_node_slots{16'384u};
        std::uint32_t maximum_edges{65'536u};
        std::uint32_t maximum_name_bytes{256u};
        std::uint32_t maximum_category_bytes{128u};
        std::uint32_t maximum_history_operations{16'384u};
        std::uint64_t maximum_node_cost_units{1'000'000'000ull};
        std::uint64_t maximum_total_cost_units{1'000'000'000'000ull};

        friend constexpr bool operator==(
            const GraphLimits&,
            const GraphLimits&) noexcept = default;
    };

    enum class ResultCode : std::uint8_t
    {
        success,
        invalid_document,
        invalid_limits,
        invalid_handle,
        missing_node,
        stale_handle,
        empty_name,
        name_too_long,
        empty_category,
        category_too_long,
        invalid_text,
        invalid_estimated_cost,
        total_cost_limit_exceeded,
        node_limit_exceeded,
        node_slot_limit_exceeded,
        edge_limit_exceeded,
        duplicate_edge,
        missing_edge,
        self_edge,
        cycle_detected,
        history_limit_exceeded,
        nothing_to_undo,
        nothing_to_redo,
        no_change,
        invalid_snapshot_schema,
        invalid_snapshot_document,
        invalid_snapshot_revision,
        malformed_snapshot
    };

    [[nodiscard]] constexpr std::string_view result_code_name(
        ResultCode code) noexcept
    {
        switch (code)
        {
        case ResultCode::success: return "success";
        case ResultCode::invalid_document: return "invalid_document";
        case ResultCode::invalid_limits: return "invalid_limits";
        case ResultCode::invalid_handle: return "invalid_handle";
        case ResultCode::missing_node: return "missing_node";
        case ResultCode::stale_handle: return "stale_handle";
        case ResultCode::empty_name: return "empty_name";
        case ResultCode::name_too_long: return "name_too_long";
        case ResultCode::empty_category: return "empty_category";
        case ResultCode::category_too_long: return "category_too_long";
        case ResultCode::invalid_text: return "invalid_text";
        case ResultCode::invalid_estimated_cost: return "invalid_estimated_cost";
        case ResultCode::total_cost_limit_exceeded:
            return "total_cost_limit_exceeded";
        case ResultCode::node_limit_exceeded: return "node_limit_exceeded";
        case ResultCode::node_slot_limit_exceeded:
            return "node_slot_limit_exceeded";
        case ResultCode::edge_limit_exceeded: return "edge_limit_exceeded";
        case ResultCode::duplicate_edge: return "duplicate_edge";
        case ResultCode::missing_edge: return "missing_edge";
        case ResultCode::self_edge: return "self_edge";
        case ResultCode::cycle_detected: return "cycle_detected";
        case ResultCode::history_limit_exceeded:
            return "history_limit_exceeded";
        case ResultCode::nothing_to_undo: return "nothing_to_undo";
        case ResultCode::nothing_to_redo: return "nothing_to_redo";
        case ResultCode::no_change: return "no_change";
        case ResultCode::invalid_snapshot_schema:
            return "invalid_snapshot_schema";
        case ResultCode::invalid_snapshot_document:
            return "invalid_snapshot_document";
        case ResultCode::invalid_snapshot_revision:
            return "invalid_snapshot_revision";
        case ResultCode::malformed_snapshot: return "malformed_snapshot";
        default: return "unknown";
        }
    }

    struct ValidationIssue final
    {
        ResultCode code{ResultCode::success};
        NodeHandle node{};
        Edge edge{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code != ResultCode::success;
        }

        friend constexpr bool operator==(
            const ValidationIssue&,
            const ValidationIssue&) noexcept = default;
    };

    struct MutationResult final
    {
        ResultCode code{ResultCode::invalid_document};
        NodeHandle node{};
        OperationId operation{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    struct NodeView final
    {
        NodeHandle handle{};
        NodeDescriptor descriptor{};

        friend bool operator==(
            const NodeView&,
            const NodeView&) noexcept = default;
    };

    struct NodeSlotSnapshot final
    {
        std::uint32_t generation{1u};
        std::optional<NodeDescriptor> descriptor{};

        friend bool operator==(
            const NodeSlotSnapshot&,
            const NodeSlotSnapshot&) noexcept = default;
    };

    struct GraphSnapshot final
    {
        std::uint32_t schema_version{kSchemaVersion};
        DocumentHandle document{};
        BranchIdentity branch{};
        DocumentRevision revision{};
        std::vector<NodeSlotSnapshot> slots{};
        std::vector<Edge> edges{};

        friend bool operator==(
            const GraphSnapshot&,
            const GraphSnapshot&) noexcept = default;
    };

    struct NodeAddedOperation final
    {
        NodeHandle handle{};
        NodeDescriptor descriptor{};
    };

    struct NodeRemovedOperation final
    {
        NodeHandle handle{};
        NodeDescriptor descriptor{};
        std::vector<Edge> incident_edges{};
    };

    struct NodeRenamedOperation final
    {
        NodeHandle handle{};
        std::string before{};
        std::string after{};
    };

    struct NodeDescriptorChangedOperation final
    {
        NodeHandle handle{};
        NodeDescriptor before{};
        NodeDescriptor after{};
    };

    struct EdgeConnectedOperation final
    {
        Edge edge{};
    };

    struct EdgeDisconnectedOperation final
    {
        Edge edge{};
    };

    using OperationPayload = std::variant<
        NodeAddedOperation,
        NodeRemovedOperation,
        NodeRenamedOperation,
        NodeDescriptorChangedOperation,
        EdgeConnectedOperation,
        EdgeDisconnectedOperation>;

    enum class OperationKind : std::uint8_t
    {
        node_added,
        node_removed,
        node_renamed,
        node_descriptor_changed,
        edge_connected,
        edge_disconnected
    };

    [[nodiscard]] constexpr std::string_view operation_kind_name(
        OperationKind kind) noexcept
    {
        switch (kind)
        {
        case OperationKind::node_added: return "node_added";
        case OperationKind::node_removed: return "node_removed";
        case OperationKind::node_renamed: return "node_renamed";
        case OperationKind::node_descriptor_changed: return "node_descriptor_changed";
        case OperationKind::edge_connected: return "edge_connected";
        case OperationKind::edge_disconnected: return "edge_disconnected";
        default: return "unknown";
        }
    }

    [[nodiscard]] inline OperationKind operation_kind(
        const OperationPayload& payload) noexcept
    {
        return std::visit([](const auto& operation) noexcept
        {
            using Type = std::remove_cvref_t<decltype(operation)>;
            if constexpr (std::same_as<Type, NodeAddedOperation>)
                return OperationKind::node_added;
            else if constexpr (std::same_as<Type, NodeRemovedOperation>)
                return OperationKind::node_removed;
            else if constexpr (std::same_as<Type, NodeRenamedOperation>)
                return OperationKind::node_renamed;
            else if constexpr (std::same_as<Type, NodeDescriptorChangedOperation>)
                return OperationKind::node_descriptor_changed;
            else if constexpr (std::same_as<Type, EdgeConnectedOperation>)
                return OperationKind::edge_connected;
            else
                return OperationKind::edge_disconnected;
        }, payload);
    }

    struct OperationRecord final
    {
        OperationId id{};
        OperationPayload payload{};
        bool applied{};

        [[nodiscard]] OperationKind kind() const noexcept
        {
            return operation_kind(payload);
        }
    };

    struct TopologicalOrder final
    {
        ResultCode code{ResultCode::invalid_document};
        std::vector<NodeHandle> nodes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    struct CriticalPath final
    {
        ResultCode code{ResultCode::invalid_document};
        std::uint64_t estimated_cost_units{};
        std::vector<NodeHandle> nodes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    struct ParallelWave final
    {
        std::uint32_t index{};
        std::uint64_t total_work_units{};
        std::uint64_t barrier_cost_units{};
        std::vector<NodeHandle> nodes{};

        friend bool operator==(
            const ParallelWave&,
            const ParallelWave&) noexcept = default;
    };

    struct ParallelWaveSimulation final
    {
        ResultCode code{ResultCode::invalid_document};
        std::uint64_t total_work_units{};
        std::uint64_t barrier_elapsed_units{};
        std::uint32_t maximum_parallel_width{};
        std::vector<ParallelWave> waves{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    struct NodeDiagnostic final
    {
        NodeHandle handle{};
        std::string name{};
        std::string category{};
        std::uint64_t estimated_cost_units{};
        std::uint32_t prerequisite_count{};
        std::uint32_t dependent_count{};
        std::uint32_t parallel_wave{};
        bool on_critical_path{};

        friend bool operator==(
            const NodeDiagnostic&,
            const NodeDiagnostic&) noexcept = default;
    };

    struct GraphMetrics final
    {
        std::uint32_t active_nodes{};
        std::uint32_t allocated_slots{};
        std::uint32_t edges{};
        std::uint32_t parallel_waves{};
        std::uint32_t maximum_parallel_width{};
        std::uint64_t total_work_units{};
        std::uint64_t critical_path_units{};
        std::uint64_t wave_barrier_units{};
        std::uint32_t retained_operations{};
        std::uint32_t applied_operations{};

        friend constexpr bool operator==(
            const GraphMetrics&,
            const GraphMetrics&) noexcept = default;
    };

    class GraphDiagnostics final
    {
    public:
        [[nodiscard]] const DocumentRevision& revision() const noexcept
        {
            return revision_;
        }

        [[nodiscard]] const GraphMetrics& metrics() const noexcept
        {
            return metrics_;
        }

        [[nodiscard]] std::span<const NodeDiagnostic> nodes() const noexcept
        {
            return nodes_;
        }

        [[nodiscard]] std::span<const Edge> edges() const noexcept
        {
            return edges_;
        }

        [[nodiscard]] std::span<const ValidationIssue> issues() const noexcept
        {
            return issues_;
        }

        [[nodiscard]] bool healthy() const noexcept
        {
            return issues_.empty();
        }

    private:
        friend class TaskGraphDocument;

        DocumentRevision revision_{};
        GraphMetrics metrics_{};
        std::vector<NodeDiagnostic> nodes_{};
        std::vector<Edge> edges_{};
        std::vector<ValidationIssue> issues_{};
    };

    class TaskGraphDocument final
    {
    public:
        explicit TaskGraphDocument(
            DocumentHandle document,
            BranchIdentity branch,
            GraphLimits limits = {})
            : document_(document)
            , branch_(branch)
            , limits_(limits)
        {
            valid_ = document_.valid()
                && branch_.valid()
                && valid_limits(limits_);
            if (valid_)
                advance_revision();
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return valid_;
        }

        [[nodiscard]] const DocumentHandle& document() const noexcept
        {
            return document_;
        }

        [[nodiscard]] const BranchIdentity& branch() const noexcept
        {
            return branch_;
        }

        [[nodiscard]] const DocumentRevision& revision() const noexcept
        {
            return revision_;
        }

        [[nodiscard]] const GraphLimits& limits() const noexcept
        {
            return limits_;
        }

        [[nodiscard]] std::vector<NodeView> nodes() const
        {
            std::vector<NodeView> result{};
            result.reserve(active_node_count(state_));
            for (std::uint32_t index = 0u; index < state_.slots.size(); ++index)
            {
                const NodeSlot& slot = state_.slots[index];
                if (slot.descriptor)
                {
                    result.push_back(NodeView{
                        NodeHandle{index, slot.generation},
                        *slot.descriptor
                    });
                }
            }
            return result;
        }

        [[nodiscard]] std::span<const Edge> edges() const noexcept
        {
            return state_.edges;
        }

        [[nodiscard]] std::optional<NodeDescriptor> node(
            NodeHandle handle) const
        {
            if (node_status(state_, handle) != ResultCode::success)
                return std::nullopt;
            return state_.slots[handle.index].descriptor;
        }

        [[nodiscard]] MutationResult add_node(NodeDescriptor descriptor)
        {
            if (!valid_)
                return failure(ResultCode::invalid_document);
            if (const ResultCode code = validate_descriptor(descriptor);
                code != ResultCode::success)
            {
                return failure(code);
            }
            if (active_node_count(state_) >= limits_.maximum_active_nodes)
                return failure(ResultCode::node_limit_exceeded);
            if (!history_can_commit())
                return failure(ResultCode::history_limit_exceeded);

            State candidate = state_;
            NodeHandle handle{};
            for (std::uint32_t index = 0u; index < candidate.slots.size(); ++index)
            {
                NodeSlot& slot = candidate.slots[index];
                if (slot.descriptor)
                    continue;

                std::uint32_t generation = slot.generation;
                const std::uint32_t issued = index < issued_generations_.size()
                    ? issued_generations_[index]
                    : 0u;
                if (issued >= generation)
                {
                    generation = authoring::next_generation(issued);
                }
                slot.generation = generation;
                slot.descriptor = descriptor;
                handle = NodeHandle{index, generation};
                break;
            }

            if (!handle)
            {
                if (candidate.slots.size() >= limits_.maximum_node_slots)
                    return failure(ResultCode::node_slot_limit_exceeded);
                const std::uint32_t index = static_cast<std::uint32_t>(
                    candidate.slots.size());
                const std::uint32_t issued = index < issued_generations_.size()
                    ? issued_generations_[index]
                    : 0u;
                const std::uint32_t generation = authoring::next_generation(
                    issued);
                candidate.slots.push_back(NodeSlot{generation, descriptor});
                handle = NodeHandle{index, generation};
            }

            if (const ValidationIssue issue = validate_state(candidate); issue)
                return failure(issue.code, issue.node);

            ensure_generation_capacity(handle.index + 1u);
            issued_generations_[handle.index] = handle.generation;

            return commit(
                NodeAddedOperation{handle, std::move(descriptor)},
                std::move(candidate),
                handle);
        }

        [[nodiscard]] MutationResult remove_node(NodeHandle handle)
        {
            if (!valid_)
                return failure(ResultCode::invalid_document, handle);
            if (const ResultCode code = node_status(state_, handle);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }
            if (!history_can_commit())
                return failure(ResultCode::history_limit_exceeded, handle);

            State candidate = state_;
            NodeRemovedOperation operation{};
            operation.handle = handle;
            operation.descriptor = *candidate.slots[handle.index].descriptor;
            for (const Edge& edge : candidate.edges)
            {
                if (edge.prerequisite == handle || edge.dependent == handle)
                    operation.incident_edges.push_back(edge);
            }
            std::erase_if(candidate.edges, [handle](const Edge& edge)
            {
                return edge.prerequisite == handle || edge.dependent == handle;
            });
            NodeSlot& slot = candidate.slots[handle.index];
            slot.descriptor.reset();
            slot.generation = authoring::next_generation(slot.generation);
            return commit(std::move(operation), std::move(candidate), handle);
        }

        [[nodiscard]] MutationResult rename_node(
            NodeHandle handle,
            std::string name)
        {
            if (!valid_)
                return failure(ResultCode::invalid_document, handle);
            if (const ResultCode code = node_status(state_, handle);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }
            if (const ResultCode code = validate_text(
                    name,
                    limits_.maximum_name_bytes,
                    ResultCode::empty_name,
                    ResultCode::name_too_long);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }

            const std::string before = state_.slots[handle.index].descriptor->name;
            if (before == name)
                return failure(ResultCode::no_change, handle);
            if (!history_can_commit())
                return failure(ResultCode::history_limit_exceeded, handle);

            State candidate = state_;
            candidate.slots[handle.index].descriptor->name = name;
            return commit(
                NodeRenamedOperation{handle, before, std::move(name)},
                std::move(candidate),
                handle);
        }

        [[nodiscard]] MutationResult update_node(
            NodeHandle handle,
            NodeDescriptor descriptor)
        {
            if (!valid_)
                return failure(ResultCode::invalid_document, handle);
            if (const ResultCode code = node_status(state_, handle);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }
            if (const ResultCode code = validate_descriptor(descriptor);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }

            const NodeDescriptor before =
                *state_.slots[handle.index].descriptor;
            if (before == descriptor)
                return failure(ResultCode::no_change, handle);
            if (!history_can_commit())
                return failure(ResultCode::history_limit_exceeded, handle);

            State candidate = state_;
            candidate.slots[handle.index].descriptor = descriptor;
            if (const ValidationIssue issue = validate_state(candidate); issue)
                return failure(issue.code, handle);
            return commit(
                NodeDescriptorChangedOperation{
                    handle,
                    before,
                    std::move(descriptor)},
                std::move(candidate),
                handle);
        }
        [[nodiscard]] MutationResult connect(
            NodeHandle prerequisite,
            NodeHandle dependent)
        {
            if (!valid_)
                return failure(ResultCode::invalid_document);
            if (const ResultCode code = node_status(state_, prerequisite);
                code != ResultCode::success)
            {
                return failure(code, prerequisite);
            }
            if (const ResultCode code = node_status(state_, dependent);
                code != ResultCode::success)
            {
                return failure(code, dependent);
            }
            const Edge edge{prerequisite, dependent};
            if (prerequisite == dependent)
                return failure(ResultCode::self_edge, prerequisite);
            if (std::binary_search(state_.edges.begin(), state_.edges.end(), edge))
                return failure(ResultCode::duplicate_edge, dependent);
            if (state_.edges.size() >= limits_.maximum_edges)
                return failure(ResultCode::edge_limit_exceeded, dependent);
            if (!history_can_commit())
                return failure(ResultCode::history_limit_exceeded, dependent);

            State candidate = state_;
            candidate.edges.insert(
                std::lower_bound(candidate.edges.begin(), candidate.edges.end(), edge),
                edge);
            if (!topological_order(candidate))
                return failure(ResultCode::cycle_detected, dependent);
            return commit(
                EdgeConnectedOperation{edge},
                std::move(candidate),
                dependent);
        }

        [[nodiscard]] MutationResult disconnect(
            NodeHandle prerequisite,
            NodeHandle dependent)
        {
            if (!valid_)
                return failure(ResultCode::invalid_document);
            if (const ResultCode code = node_status(state_, prerequisite);
                code != ResultCode::success)
            {
                return failure(code, prerequisite);
            }
            if (const ResultCode code = node_status(state_, dependent);
                code != ResultCode::success)
            {
                return failure(code, dependent);
            }
            const Edge edge{prerequisite, dependent};
            if (prerequisite == dependent)
                return failure(ResultCode::self_edge, prerequisite);
            const auto found = std::lower_bound(
                state_.edges.begin(), state_.edges.end(), edge);
            if (found == state_.edges.end() || *found != edge)
                return failure(ResultCode::missing_edge, dependent);
            if (!history_can_commit())
                return failure(ResultCode::history_limit_exceeded, dependent);

            State candidate = state_;
            candidate.edges.erase(
                candidate.edges.begin() + (found - state_.edges.begin()));
            return commit(
                EdgeDisconnectedOperation{edge},
                std::move(candidate),
                dependent);
        }

        [[nodiscard]] bool can_undo() const noexcept
        {
            return history_cursor_ != 0u;
        }

        [[nodiscard]] bool can_redo() const noexcept
        {
            return history_cursor_ < history_.size();
        }

        [[nodiscard]] MutationResult undo()
        {
            if (!valid_)
                return failure(ResultCode::invalid_document);
            if (!can_undo())
                return failure(ResultCode::nothing_to_undo);
            const HistoryEntry& entry = history_[history_cursor_ - 1u];
            state_ = entry.before;
            --history_cursor_;
            advance_revision();
            return MutationResult{ResultCode::success, {}, entry.record.id};
        }

        [[nodiscard]] MutationResult redo()
        {
            if (!valid_)
                return failure(ResultCode::invalid_document);
            if (!can_redo())
                return failure(ResultCode::nothing_to_redo);
            const HistoryEntry& entry = history_[history_cursor_];
            state_ = entry.after;
            ++history_cursor_;
            advance_revision();
            return MutationResult{ResultCode::success, {}, entry.record.id};
        }

        [[nodiscard]] std::vector<OperationRecord> history() const
        {
            std::vector<OperationRecord> result{};
            result.reserve(history_.size());
            for (std::size_t index = 0u; index < history_.size(); ++index)
            {
                OperationRecord record = history_[index].record;
                record.applied = index < history_cursor_;
                result.push_back(std::move(record));
            }
            return result;
        }

        [[nodiscard]] TopologicalOrder topological_order() const
        {
            if (!valid_)
                return TopologicalOrder{ResultCode::invalid_document, {}};
            return topological_order(state_);
        }

        [[nodiscard]] CriticalPath critical_path() const
        {
            if (!valid_)
                return CriticalPath{ResultCode::invalid_document, 0u, {}};
            const TopologicalOrder order = topological_order(state_);
            if (!order)
                return CriticalPath{order.code, 0u, {}};
            if (order.nodes.empty())
                return CriticalPath{ResultCode::success, 0u, {}};

            const DependencyIndex dependencies = dependency_index(state_);
            std::vector<std::uint64_t> finish(state_.slots.size(), 0u);
            std::vector<NodeHandle> predecessor(state_.slots.size());
            for (const NodeHandle handle : order.nodes)
            {
                const std::uint64_t ownCost =
                    state_.slots[handle.index].descriptor->estimated_cost_units;
                finish[handle.index] = ownCost;
                for (const NodeHandle candidatePredecessor
                    : dependencies.incoming[handle.index])
                {
                    const std::uint64_t candidate =
                        finish[candidatePredecessor.index] + ownCost;
                    if (candidate > finish[handle.index]
                        || (candidate == finish[handle.index]
                            && (!predecessor[handle.index]
                                || candidatePredecessor
                                    < predecessor[handle.index])))
                    {
                        finish[handle.index] = candidate;
                        predecessor[handle.index] = candidatePredecessor;
                    }
                }
            }

            NodeHandle end = order.nodes.front();
            for (const NodeHandle handle : order.nodes)
            {
                if (finish[handle.index] > finish[end.index]
                    || (finish[handle.index] == finish[end.index]
                        && handle < end))
                {
                    end = handle;
                }
            }

            CriticalPath result{ResultCode::success, finish[end.index], {}};
            for (NodeHandle current = end; current;)
            {
                result.nodes.push_back(current);
                current = predecessor[current.index];
            }
            std::reverse(result.nodes.begin(), result.nodes.end());
            return result;
        }

        [[nodiscard]] ParallelWaveSimulation simulate_parallel_waves() const
        {
            if (!valid_)
            {
                return ParallelWaveSimulation{
                    ResultCode::invalid_document, 0u, 0u, 0u, {}};
            }
            const TopologicalOrder order = topological_order(state_);
            if (!order)
                return ParallelWaveSimulation{order.code, 0u, 0u, 0u, {}};

            ParallelWaveSimulation result{};
            result.code = ResultCode::success;
            const DependencyIndex dependencies = dependency_index(state_);
            std::vector<std::uint32_t> waveIndex(state_.slots.size(), 0u);
            for (const NodeHandle handle : order.nodes)
            {
                std::uint32_t index = 0u;
                for (const NodeHandle prerequisite
                    : dependencies.incoming[handle.index])
                {
                    index = (std::max)(
                        index,
                        waveIndex[prerequisite.index] + 1u);
                }
                waveIndex[handle.index] = index;
                if (result.waves.size() <= index)
                    result.waves.resize(index + 1u);
                ParallelWave& wave = result.waves[index];
                wave.index = index;
                wave.nodes.push_back(handle);
                const std::uint64_t cost =
                    state_.slots[handle.index].descriptor->estimated_cost_units;
                wave.total_work_units += cost;
                wave.barrier_cost_units = (std::max)(wave.barrier_cost_units, cost);
                result.total_work_units += cost;
            }
            for (const ParallelWave& wave : result.waves)
            {
                result.barrier_elapsed_units += wave.barrier_cost_units;
                result.maximum_parallel_width = (std::max)(
                    result.maximum_parallel_width,
                    static_cast<std::uint32_t>(wave.nodes.size()));
            }
            return result;
        }

        [[nodiscard]] GraphSnapshot snapshot() const
        {
            GraphSnapshot result{};
            result.document = document_;
            result.branch = branch_;
            result.revision = revision_;
            result.edges = state_.edges;
            result.slots.reserve(state_.slots.size());
            for (const NodeSlot& slot : state_.slots)
            {
                result.slots.push_back(NodeSlotSnapshot{
                    slot.generation,
                    slot.descriptor
                });
            }
            return result;
        }

        [[nodiscard]] MutationResult restore_snapshot(
            const GraphSnapshot& snapshot)
        {
            if (!valid_)
                return failure(ResultCode::invalid_document);
            if (snapshot.schema_version != kSchemaVersion)
                return failure(ResultCode::invalid_snapshot_schema);
            if (snapshot.document != document_ || snapshot.branch != branch_)
                return failure(ResultCode::invalid_snapshot_document);
            if (!snapshot.revision.valid())
                return failure(ResultCode::invalid_snapshot_revision);
            if (snapshot.slots.size() > limits_.maximum_node_slots
                || snapshot.edges.size() > limits_.maximum_edges)
            {
                return failure(ResultCode::malformed_snapshot);
            }

            State candidate{};
            candidate.edges = snapshot.edges;
            candidate.slots.reserve(snapshot.slots.size());
            for (const NodeSlotSnapshot& slot : snapshot.slots)
                candidate.slots.push_back(NodeSlot{slot.generation, slot.descriptor});
            std::sort(candidate.edges.begin(), candidate.edges.end());
            if (const ValidationIssue issue = validate_state(candidate); issue)
                return failure(ResultCode::malformed_snapshot, issue.node);
            const authoring::ContentHash candidateHash = content_hash(candidate);
            if (!std::equal(
                    candidateHash.words.begin(),
                    candidateHash.words.end(),
                    snapshot.revision.content.words.begin()))
            {
                return failure(ResultCode::invalid_snapshot_revision);
            }

            state_ = std::move(candidate);
            history_.clear();
            history_cursor_ = 0u;
            ensure_generation_capacity(
                static_cast<std::uint32_t>(state_.slots.size()));
            for (std::uint32_t index = 0u; index < state_.slots.size(); ++index)
            {
                const NodeSlot& slot = state_.slots[index];
                const std::uint32_t issued = slot.descriptor
                    ? slot.generation
                    : (slot.generation > 1u ? slot.generation - 1u : 0u);
                issued_generations_[index] = (std::max)(
                    issued_generations_[index],
                    issued);
            }
            advance_revision();
            return MutationResult{ResultCode::success, {}, {}};
        }

        [[nodiscard]] ValidationIssue validate() const
        {
            if (!valid_)
                return ValidationIssue{ResultCode::invalid_document, {}, {}};
            return validate_state(state_);
        }

        [[nodiscard]] GraphDiagnostics diagnostics() const
        {
            GraphDiagnostics result{};
            result.revision_ = revision_;
            const ValidationIssue issue = validate();
            if (issue)
                result.issues_.push_back(issue);
            result.edges_ = state_.edges;

            const CriticalPath critical = critical_path();
            const ParallelWaveSimulation waves = simulate_parallel_waves();
            result.metrics_.active_nodes = active_node_count(state_);
            result.metrics_.allocated_slots = static_cast<std::uint32_t>(
                state_.slots.size());
            result.metrics_.edges = static_cast<std::uint32_t>(state_.edges.size());
            result.metrics_.retained_operations = static_cast<std::uint32_t>(
                history_.size());
            result.metrics_.applied_operations = static_cast<std::uint32_t>(
                history_cursor_);
            if (critical)
                result.metrics_.critical_path_units = critical.estimated_cost_units;
            if (waves)
            {
                result.metrics_.parallel_waves = static_cast<std::uint32_t>(
                    waves.waves.size());
                result.metrics_.maximum_parallel_width = waves.maximum_parallel_width;
                result.metrics_.total_work_units = waves.total_work_units;
                result.metrics_.wave_barrier_units = waves.barrier_elapsed_units;
            }

            const auto isCritical = [&critical](NodeHandle handle)
            {
                return std::find(
                    critical.nodes.begin(), critical.nodes.end(), handle)
                    != critical.nodes.end();
            };
            std::vector<std::uint32_t> prerequisiteCount(
                state_.slots.size(), 0u);
            std::vector<std::uint32_t> dependentCount(
                state_.slots.size(), 0u);
            std::vector<std::uint32_t> waveIndex(state_.slots.size(), 0u);
            for (const Edge& edge : state_.edges)
            {
                ++prerequisiteCount[edge.dependent.index];
                ++dependentCount[edge.prerequisite.index];
            }
            if (waves)
            {
                for (const ParallelWave& wave : waves.waves)
                {
                    for (const NodeHandle handle : wave.nodes)
                        waveIndex[handle.index] = wave.index;
                }
            }
            for (const NodeView& view : nodes())
            {
                NodeDiagnostic diagnostic{};
                diagnostic.handle = view.handle;
                diagnostic.name = view.descriptor.name;
                diagnostic.category = view.descriptor.category;
                diagnostic.estimated_cost_units =
                    view.descriptor.estimated_cost_units;
                diagnostic.on_critical_path = critical && isCritical(view.handle);
                diagnostic.prerequisite_count =
                    prerequisiteCount[view.handle.index];
                diagnostic.dependent_count = dependentCount[view.handle.index];
                diagnostic.parallel_wave = waveIndex[view.handle.index];
                result.nodes_.push_back(std::move(diagnostic));
            }
            return result;
        }

    private:
        struct NodeSlot final
        {
            std::uint32_t generation{1u};
            std::optional<NodeDescriptor> descriptor{};
        };

        struct State final
        {
            std::vector<NodeSlot> slots{};
            std::vector<Edge> edges{};
        };

        struct DependencyIndex final
        {
            ResultCode code{ResultCode::success};
            std::vector<std::uint32_t> indegree{};
            std::vector<std::vector<NodeHandle>> incoming{};
            std::vector<std::vector<NodeHandle>> outgoing{};
        };

        struct HistoryEntry final
        {
            OperationRecord record{};
            State before{};
            State after{};
        };

        [[nodiscard]] static bool valid_limits(
            const GraphLimits& limits) noexcept
        {
            return limits.maximum_active_nodes != 0u
                && limits.maximum_node_slots >= limits.maximum_active_nodes
                && limits.maximum_edges != 0u
                && limits.maximum_name_bytes != 0u
                && limits.maximum_category_bytes != 0u
                && limits.maximum_history_operations != 0u
                && limits.maximum_node_cost_units != 0u
                && limits.maximum_total_cost_units
                    >= limits.maximum_node_cost_units;
        }

        [[nodiscard]] static bool text_has_visible_character(
            std::string_view text) noexcept
        {
            for (const unsigned char character : text)
            {
                if (character == 0u)
                    return false;
                if (character > 0x20u && character != 0x7fu)
                    return true;
            }
            return false;
        }

        [[nodiscard]] static ResultCode validate_text(
            std::string_view text,
            std::uint32_t maximumBytes,
            ResultCode emptyCode,
            ResultCode tooLongCode) noexcept
        {
            if (text.empty() || !text_has_visible_character(text))
                return emptyCode;
            if (text.size() > maximumBytes)
                return tooLongCode;
            if (std::find(text.begin(), text.end(), '\0') != text.end())
                return ResultCode::invalid_text;
            return ResultCode::success;
        }

        [[nodiscard]] ResultCode validate_descriptor(
            const NodeDescriptor& descriptor) const noexcept
        {
            if (const ResultCode code = validate_text(
                    descriptor.name,
                    limits_.maximum_name_bytes,
                    ResultCode::empty_name,
                    ResultCode::name_too_long);
                code != ResultCode::success)
            {
                return code;
            }
            if (const ResultCode code = validate_text(
                    descriptor.category,
                    limits_.maximum_category_bytes,
                    ResultCode::empty_category,
                    ResultCode::category_too_long);
                code != ResultCode::success)
            {
                return code;
            }
            if (descriptor.estimated_cost_units == 0u
                || descriptor.estimated_cost_units
                    > limits_.maximum_node_cost_units)
            {
                return ResultCode::invalid_estimated_cost;
            }
            return ResultCode::success;
        }

        [[nodiscard]] static std::uint32_t active_node_count(
            const State& state) noexcept
        {
            return static_cast<std::uint32_t>(std::count_if(
                state.slots.begin(), state.slots.end(),
                [](const NodeSlot& slot) { return slot.descriptor.has_value(); }));
        }

        [[nodiscard]] static ResultCode node_status(
            const State& state,
            NodeHandle handle) noexcept
        {
            if (!handle.valid())
                return ResultCode::invalid_handle;
            if (handle.index >= state.slots.size())
                return ResultCode::missing_node;
            const NodeSlot& slot = state.slots[handle.index];
            if (slot.generation != handle.generation)
                return ResultCode::stale_handle;
            if (!slot.descriptor)
                return ResultCode::missing_node;
            return ResultCode::success;
        }

        [[nodiscard]] TopologicalOrder topological_order(
            const State& state) const
        {
            DependencyIndex dependencies = dependency_index(state);
            if (dependencies.code != ResultCode::success)
                return TopologicalOrder{dependencies.code, {}};

            std::priority_queue<
                NodeHandle,
                std::vector<NodeHandle>,
                std::greater<NodeHandle>> ready{};
            for (std::uint32_t index = 0u; index < state.slots.size(); ++index)
            {
                const NodeSlot& slot = state.slots[index];
                if (slot.descriptor && dependencies.indegree[index] == 0u)
                    ready.push(NodeHandle{index, slot.generation});
            }

            TopologicalOrder result{ResultCode::success, {}};
            result.nodes.reserve(active_node_count(state));
            while (!ready.empty())
            {
                const NodeHandle current = ready.top();
                ready.pop();
                result.nodes.push_back(current);
                for (const NodeHandle dependent
                    : dependencies.outgoing[current.index])
                {
                    if (--dependencies.indegree[dependent.index] == 0u)
                        ready.push(dependent);
                }
            }
            if (result.nodes.size() != active_node_count(state))
            {
                result.code = ResultCode::cycle_detected;
                result.nodes.clear();
            }
            return result;
        }

        [[nodiscard]] static DependencyIndex dependency_index(
            const State& state)
        {
            DependencyIndex result{};
            result.indegree.resize(state.slots.size(), 0u);
            result.incoming.resize(state.slots.size());
            result.outgoing.resize(state.slots.size());
            for (const Edge& edge : state.edges)
            {
                if (node_status(state, edge.prerequisite) != ResultCode::success
                    || node_status(state, edge.dependent) != ResultCode::success)
                {
                    result.code = ResultCode::malformed_snapshot;
                    return result;
                }
                ++result.indegree[edge.dependent.index];
                result.incoming[edge.dependent.index].push_back(
                    edge.prerequisite);
                result.outgoing[edge.prerequisite.index].push_back(
                    edge.dependent);
            }
            for (auto& incoming : result.incoming)
                std::sort(incoming.begin(), incoming.end());
            for (auto& outgoing : result.outgoing)
                std::sort(outgoing.begin(), outgoing.end());
            return result;
        }

        [[nodiscard]] ValidationIssue validate_state(
            const State& state) const
        {
            if (state.slots.size() > limits_.maximum_node_slots)
                return {ResultCode::node_slot_limit_exceeded, {}, {}};
            if (active_node_count(state) > limits_.maximum_active_nodes)
                return {ResultCode::node_limit_exceeded, {}, {}};
            if (state.edges.size() > limits_.maximum_edges)
                return {ResultCode::edge_limit_exceeded, {}, {}};

            std::uint64_t totalCost = 0u;
            for (std::uint32_t index = 0u; index < state.slots.size(); ++index)
            {
                const NodeSlot& slot = state.slots[index];
                if (slot.generation == 0u)
                {
                    return {
                        ResultCode::malformed_snapshot,
                        NodeHandle{index, 0u},
                        {}};
                }
                if (!slot.descriptor)
                    continue;
                const NodeHandle handle{index, slot.generation};
                if (const ResultCode code = validate_descriptor(*slot.descriptor);
                    code != ResultCode::success)
                {
                    return {code, handle, {}};
                }
                const std::uint64_t cost =
                    slot.descriptor->estimated_cost_units;
                if (cost > limits_.maximum_total_cost_units - totalCost)
                {
                    return {
                        ResultCode::total_cost_limit_exceeded,
                        handle,
                        {}};
                }
                totalCost += cost;
            }

            std::vector<Edge> sorted = state.edges;
            std::sort(sorted.begin(), sorted.end());
            for (std::size_t index = 0u; index < sorted.size(); ++index)
            {
                const Edge& edge = sorted[index];
                if (edge.prerequisite == edge.dependent)
                    return {ResultCode::self_edge, edge.prerequisite, edge};
                if (const ResultCode code = node_status(state, edge.prerequisite);
                    code != ResultCode::success)
                {
                    return {code, edge.prerequisite, edge};
                }
                if (const ResultCode code = node_status(state, edge.dependent);
                    code != ResultCode::success)
                {
                    return {code, edge.dependent, edge};
                }
                if (index != 0u && sorted[index - 1u] == edge)
                    return {ResultCode::duplicate_edge, edge.dependent, edge};
            }
            if (!topological_order(state))
                return {ResultCode::cycle_detected, {}, {}};
            return {};
        }

        [[nodiscard]] authoring::ContentHash content_hash(
            const State& state) const noexcept
        {
            authoring::DeterministicHashBuilder hash{
                "epoch.authoring.task_graph.v1"};
            hash.append_u32(kSchemaVersion);
            hash.append_u64(static_cast<std::uint64_t>(state.slots.size()));
            for (std::uint32_t index = 0u; index < state.slots.size(); ++index)
            {
                const NodeSlot& slot = state.slots[index];
                hash.append_u32(index);
                hash.append_u32(slot.generation);
                hash.append_bool(slot.descriptor.has_value());
                if (slot.descriptor)
                {
                    hash.append_string(slot.descriptor->name);
                    hash.append_string(slot.descriptor->category);
                    hash.append_u64(slot.descriptor->estimated_cost_units);
                }
            }
            hash.append_u64(static_cast<std::uint64_t>(state.edges.size()));
            for (const Edge& edge : state.edges)
            {
                hash.append_handle(edge.prerequisite);
                hash.append_handle(edge.dependent);
            }
            return hash.finish();
        }

        void advance_revision() noexcept
        {
            ++revision_sequence_;
            if (revision_sequence_ == 0u)
                revision_sequence_ = 1u;
            revision_ = DocumentRevision{
                content_hash(state_),
                revision_sequence_
            };
        }

        void ensure_generation_capacity(std::uint32_t size)
        {
            if (issued_generations_.size() < size)
                issued_generations_.resize(size, 0u);
        }

        [[nodiscard]] bool history_can_commit() const noexcept
        {
            return history_cursor_ < limits_.maximum_history_operations;
        }

        [[nodiscard]] static MutationResult failure(
            ResultCode code,
            NodeHandle node = {}) noexcept
        {
            return MutationResult{code, node, {}};
        }

        template<typename Payload>
        [[nodiscard]] MutationResult commit(
            Payload payload,
            State candidate,
            NodeHandle node)
        {
            if (history_cursor_ < history_.size())
            {
                history_.erase(
                    history_.begin() + static_cast<std::ptrdiff_t>(history_cursor_),
                    history_.end());
            }
            const OperationId operation{next_operation_id_++};
            HistoryEntry entry{};
            entry.record = OperationRecord{
                operation,
                OperationPayload{std::move(payload)},
                true
            };
            entry.before = state_;
            entry.after = candidate;
            history_.push_back(std::move(entry));
            state_ = std::move(candidate);
            history_cursor_ = history_.size();
            advance_revision();
            return MutationResult{ResultCode::success, node, operation};
        }

        DocumentHandle document_{};
        BranchIdentity branch_{};
        GraphLimits limits_{};
        bool valid_{};
        State state_{};
        std::vector<std::uint32_t> issued_generations_{};
        std::vector<HistoryEntry> history_{};
        std::size_t history_cursor_{};
        std::uint64_t next_operation_id_{1u};
        std::uint64_t revision_sequence_{};
        DocumentRevision revision_{};
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        invalid_document,
        descriptor_validation,
        deterministic_order,
        critical_path,
        parallel_waves,
        edge_validation,
        atomic_cycle_rejection,
        stale_identity,
        semantic_history,
        history_branch_generation,
        snapshot_restore,
        snapshot_validation,
        bounded_state,
        immutable_diagnostics,
        operation_journal,
        result_names
    };

    [[nodiscard]] ContractFailure run_contract();
}
