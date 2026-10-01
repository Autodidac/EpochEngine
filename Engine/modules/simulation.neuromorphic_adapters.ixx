/************************************************
 * Epoch Engine - Neuromorphic Integration Adapters
 *
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 * Provided "AS IS", without warranty of any kind.
 * See LICENSE for full terms.
 ***********************************************/
module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

export module simulation.neuromorphic_adapters;

import authoring.task_graph;
import simulation.neuromorphic;
import timeline.system;

export namespace epochengine::simulation::neuromorphic::adapters
{
    namespace task_graph = epochengine::authoring::task_graph;
    namespace timeline = epochengine::timeline;

    [[nodiscard]] inline Signal from_timeline_event(
        const timeline::TimelineEvent& event,
        std::uint64_t sequence = 0u) noexcept
    {
        Signal signal{};
        signal.channel = channels::timeline;
        signal.semantic = combine_id(
            stable_id(event_kind_name(event.kind)),
            stable_id(event.label));
        signal.source = combine_id(
            stable_id(event.track_id),
            stable_id(event.target_name));
        signal.sequence = sequence;

        const double nonNegativeSeconds = (std::max)(0.0, event.simulated_seconds);
        const double maxSeconds = static_cast<double>(
            (std::numeric_limits<TimestampNanoseconds>::max)()) / 1'000'000'000.0;
        signal.timestamp_ns = nonNegativeSeconds >= maxSeconds
            ? (std::numeric_limits<TimestampNanoseconds>::max)()
            : static_cast<TimestampNanoseconds>(nonNegativeSeconds * 1'000'000'000.0);

        signal.magnitude = 1.0f;
        signal.salience = event.kind == timeline::TimelineEventKind::CameraCut
            ? 2.0f
            : 1.0f;
        signal.kind = SignalKind::timeline;
        signal.truth = TruthClass::exact_cause;
        signal.polarity = Polarity::excitatory;
        return signal;
    }

    struct AuthoringNodeBinding final
    {
        task_graph::NodeHandle authoring{};
        NodeHandle runtime{};
        ChannelId input_channel{};
        ChannelId output_channel{};
    };

    struct AuthoringCompileResult final
    {
        std::vector<AuthoringNodeBinding> bindings{};
        std::size_t edge_count{};
        bool valid{};
    };

    [[nodiscard]] inline const AuthoringNodeBinding* find_binding(
        const AuthoringCompileResult& result,
        task_graph::NodeHandle handle) noexcept
    {
        for (const AuthoringNodeBinding& binding : result.bindings)
        {
            if (binding.authoring == handle)
                return &binding;
        }
        return nullptr;
    }

    [[nodiscard]] inline AuthoringCompileResult compile_authoring_graph(
        const task_graph::GraphSnapshot& snapshot,
        Graph& runtime)
    {
        AuthoringCompileResult result{};
        if (!runtime.empty()
            || snapshot.schema_version != task_graph::kSchemaVersion
            || snapshot.slots.size() > runtime.limits().maximum_nodes
            || snapshot.edges.size() > runtime.limits().maximum_edges)
        {
            return result;
        }

        Graph candidate(runtime.limits());
        result.bindings.reserve(snapshot.slots.size());
        for (std::uint32_t index = 0u;
            index < static_cast<std::uint32_t>(snapshot.slots.size());
            ++index)
        {
            const auto& slot = snapshot.slots[index];
            if (!slot.descriptor || slot.generation == 0u)
                continue;

            const task_graph::NodeHandle authoringHandle{index, slot.generation};
            ChannelId inputChannel = combine_id(channels::graph_input, index);
            inputChannel = combine_id(inputChannel, slot.generation);
            ChannelId outputChannel = combine_id(channels::graph_output, index);
            outputChannel = combine_id(outputChannel, slot.generation);
            const auto semantic = combine_id(
                stable_id(slot.descriptor->category),
                stable_id(slot.descriptor->name));

            const NodeHandle runtimeHandle = candidate.add_node(NodeConfig{
                .input_channel = inputChannel,
                .output_channel = outputChannel,
                .semantic = semantic,
                .threshold = 1.0f,
                .gain = 1.0f,
                .leak_per_second = 0.0f,
                .reset_potential = 0.0f,
                .refractory_ns = 1'000u
            });
            if (!runtimeHandle)
                return {};

            result.bindings.push_back(AuthoringNodeBinding{
                .authoring = authoringHandle,
                .runtime = runtimeHandle,
                .input_channel = inputChannel,
                .output_channel = outputChannel
            });
        }

        for (const task_graph::Edge& edge : snapshot.edges)
        {
            const AuthoringNodeBinding* source = find_binding(result, edge.prerequisite);
            const AuthoringNodeBinding* target = find_binding(result, edge.dependent);
            if (!source || !target || !candidate.connect(source->runtime, target->runtime, 1.0f))
                return {};
            ++result.edge_count;
        }

        result.valid = !result.bindings.empty();
        if (result.valid)
            runtime = std::move(candidate);
        return result;
    }

    [[nodiscard]] inline bool activate_authoring_node(
        Graph& runtime,
        const AuthoringCompileResult& compiled,
        task_graph::NodeHandle node,
        Signal signal)
    {
        const AuthoringNodeBinding* binding = find_binding(compiled, node);
        if (!compiled.valid || !binding)
            return false;
        signal.target = binding->runtime;
        if (signal.channel == 0u)
            signal.channel = binding->input_channel;
        return runtime.emit(signal);
    }

    struct ContractChecks final
    {
        bool timeline_bridge{};
        bool graph_compilation{};
        bool graph_activation{};

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return timeline_bridge && graph_compilation && graph_activation;
        }
    };

    [[nodiscard]] inline ContractChecks run_contract_checks()
    {
        timeline::TimelineEvent event{};
        event.track_id = "Camera";
        event.kind = timeline::TimelineEventKind::CameraCut;
        event.simulated_seconds = 1.25;
        event.frame_index = 75u;
        event.label = "Cut to target";
        event.target_name = "Camera.Main";
        const Signal timelineSignal = from_timeline_event(event, 11u);

        task_graph::GraphSnapshot snapshot{};
        snapshot.slots.push_back(task_graph::NodeSlotSnapshot{
            .generation = 1u,
            .descriptor = task_graph::NodeDescriptor{
                .name = "Perception",
                .category = "Neuromorphic",
                .estimated_cost_units = 1u
            }
        });
        snapshot.slots.push_back(task_graph::NodeSlotSnapshot{
            .generation = 1u,
            .descriptor = task_graph::NodeDescriptor{
                .name = "Attention",
                .category = "Neuromorphic",
                .estimated_cost_units = 1u
            }
        });
        snapshot.edges.push_back(task_graph::Edge{
            .prerequisite = task_graph::NodeHandle{0u, 1u},
            .dependent = task_graph::NodeHandle{1u, 1u}
        });

        Graph graph{};
        const AuthoringCompileResult compiled = compile_authoring_graph(snapshot, graph);
        Signal activation{};
        activation.timestamp_ns = 2'000'000u;
        activation.magnitude = 1.0f;
        activation.salience = 1.0f;
        activation.kind = SignalKind::semantic;
        activation.truth = TruthClass::exact_cause;
        activation.polarity = Polarity::excitatory;
        const bool activated = activate_authoring_node(
            graph,
            compiled,
            task_graph::NodeHandle{0u, 1u},
            activation);
        const PumpResult pump = graph.pump(2'000'000u, 16u);
        const AuthoringNodeBinding* second = find_binding(
            compiled,
            task_graph::NodeHandle{1u, 1u});
        const NodeSnapshot secondSnapshot = second
            ? graph.snapshot(second->runtime)
            : NodeSnapshot{};

        return ContractChecks{
            .timeline_bridge = timelineSignal.channel == channels::timeline
                && timelineSignal.timestamp_ns == 1'250'000'000u
                && timelineSignal.sequence == 11u
                && timelineSignal.kind == SignalKind::timeline
                && timelineSignal.truth == TruthClass::exact_cause
                && timelineSignal.salience > 1.0f,
            .graph_compilation = compiled.valid
                && compiled.bindings.size() == 2u
                && compiled.edge_count == 1u
                && graph.metrics().node_count == 2u
                && graph.metrics().edge_count == 1u,
            .graph_activation = activated
                && pump.processed_activations == 2u
                && pump.fired_nodes == 2u
                && secondSnapshot.activation_count == 1u
                && secondSnapshot.fire_count == 1u
        };
    }
}
