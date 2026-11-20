/************************************************
 * Epoch Engine - Neuromorphic Runtime
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
#include <iterator>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

export module simulation.neuromorphic;

export namespace epochengine::simulation::neuromorphic
{
    using TimestampNanoseconds = std::uint64_t;
    using ChannelId = std::uint64_t;
    using SemanticId = std::uint64_t;

    [[nodiscard]] constexpr ChannelId stable_id(std::string_view text) noexcept
    {
        std::uint64_t hash = 14695981039346656037ull;
        for (const unsigned char value : text)
        {
            hash ^= value;
            hash *= 1099511628211ull;
        }
        return hash == 0u ? 1u : hash;
    }

    [[nodiscard]] constexpr ChannelId combine_id(
        ChannelId seed,
        std::uint64_t value) noexcept
    {
        seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
        return seed == 0u ? 1u : seed;
    }

    namespace channels
    {
        inline constexpr ChannelId timeline = stable_id("epoch.timeline");
        inline constexpr ChannelId event_camera = stable_id("epoch.render.event_camera");
        inline constexpr ChannelId graph_input = stable_id("epoch.graph.input");
        inline constexpr ChannelId graph_output = stable_id("epoch.graph.output");
        inline constexpr ChannelId collision = stable_id("epoch.physics.collision");
        inline constexpr ChannelId input = stable_id("epoch.input");
        inline constexpr ChannelId simulation = stable_id("epoch.simulation");
    }

    enum class SignalKind : std::uint8_t
    {
        semantic,
        sensory,
        timeline,
        input,
        collision,
        simulation
    };

    enum class TruthClass : std::uint8_t
    {
        transient,
        visual_only,
        error_bounded,
        exact_cause
    };

    enum class Polarity : std::int8_t
    {
        inhibitory = -1,
        neutral = 0,
        excitatory = 1
    };

    struct NodeHandle final
    {
        std::uint32_t index{(std::numeric_limits<std::uint32_t>::max)()};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return index != (std::numeric_limits<std::uint32_t>::max)()
                && generation != 0u;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return valid();
        }

        [[nodiscard]] friend constexpr bool operator==(
            const NodeHandle&,
            const NodeHandle&) noexcept = default;
    };

    struct Signal final
    {
        NodeHandle target{};
        ChannelId channel{};
        SemanticId semantic{};
        std::uint64_t source{};
        std::uint64_t sequence{};
        TimestampNanoseconds timestamp_ns{};
        float magnitude{1.0f};
        float salience{1.0f};
        SignalKind kind{SignalKind::semantic};
        TruthClass truth{TruthClass::transient};
        Polarity polarity{Polarity::excitatory};
    };

    struct GraphLimits final
    {
        std::size_t maximum_nodes{4'096u};
        std::size_t maximum_edges{65'536u};
        std::size_t maximum_pending_activations{65'536u};
        std::size_t maximum_routes{4'096u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_nodes > 0u
                && maximum_nodes <= 65'536u
                && maximum_edges >= maximum_nodes
                && maximum_edges <= 1'048'576u
                && maximum_pending_activations >= maximum_nodes
                && maximum_pending_activations <= 1'048'576u
                && maximum_routes > 0u
                && maximum_routes <= maximum_nodes;
        }
    };

    struct NodeConfig final
    {
        ChannelId input_channel{channels::simulation};
        ChannelId output_channel{channels::simulation};
        SemanticId semantic{};
        float threshold{1.0f};
        float gain{1.0f};
        float leak_per_second{0.0f};
        float reset_potential{0.0f};
        TimestampNanoseconds refractory_ns{1'000u};
    };

    struct Edge final
    {
        NodeHandle source{};
        NodeHandle target{};
        float weight{1.0f};
    };

    struct NodeSnapshot final
    {
        NodeHandle handle{};
        NodeConfig config{};
        float potential{};
        TimestampNanoseconds last_update_ns{};
        TimestampNanoseconds last_fire_ns{};
        std::uint64_t activation_count{};
        std::uint64_t fire_count{};
        bool active{};
    };

    struct GraphMetrics final
    {
        std::size_t node_count{};
        std::size_t edge_count{};
        std::size_t route_count{};
        std::size_t pending_count{};
        std::uint64_t emitted_signal_count{};
        std::uint64_t rejected_signal_count{};
        std::uint64_t routed_activation_count{};
        std::uint64_t processed_activation_count{};
        std::uint64_t fired_node_count{};
        std::uint64_t suppressed_refractory_count{};
        std::uint64_t dropped_activation_count{};
        TimestampNanoseconds latest_timestamp_ns{};
    };

    struct PumpResult final
    {
        std::size_t processed_activations{};
        std::size_t fired_nodes{};
        std::size_t propagated_activations{};
        std::size_t pending_activations{};
        bool budget_exhausted{};
    };

    using SpikeSink = void (*)(void* user, const Signal& signal) noexcept;

    class Graph final
    {
    public:
        explicit Graph(GraphLimits limits = {})
            : limits_(limits.valid() ? limits : GraphLimits{})
        {
            nodes_.reserve(limits_.maximum_nodes);
            routes_.reserve(limits_.maximum_routes);
            pending_.reserve(limits_.maximum_pending_activations);
        }

        Graph(const Graph&) = delete;
        Graph& operator=(const Graph&) = delete;
        Graph(Graph&&) noexcept = default;
        Graph& operator=(Graph&&) noexcept = default;

        [[nodiscard]] const GraphLimits& limits() const noexcept
        {
            return limits_;
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return metrics_.node_count == 0u;
        }

        [[nodiscard]] NodeHandle add_node(NodeConfig config)
        {
            if (!valid_config(config) || metrics_.node_count >= limits_.maximum_nodes)
                return {};

            if (route_index(config.input_channel) == routes_.size()
                && routes_.size() >= limits_.maximum_routes)
            {
                return {};
            }

            const std::uint32_t index = static_cast<std::uint32_t>(nodes_.size());
            NodeRecord record{};
            record.generation = 1u;
            record.active = true;
            record.config = config;
            record.outgoing.reserve(4u);
            nodes_.push_back(std::move(record));

            const NodeHandle handle{index, 1u};
            if (!add_route(config.input_channel, handle))
            {
                nodes_.pop_back();
                return {};
            }

            ++metrics_.node_count;
            metrics_.route_count = routes_.size();
            return handle;
        }

        [[nodiscard]] bool connect(
            NodeHandle source,
            NodeHandle target,
            float weight = 1.0f)
        {
            NodeRecord* sourceNode = node_record(source);
            if (!sourceNode || !node_record(target)
                || source == target
                || !std::isfinite(weight)
                || weight == 0.0f
                || std::abs(weight) > 16.0f
                || metrics_.edge_count >= limits_.maximum_edges)
            {
                return false;
            }

            for (const OutgoingEdge& edge : sourceNode->outgoing)
            {
                if (edge.target == target)
                    return false;
            }

            sourceNode->outgoing.push_back(OutgoingEdge{target, weight});
            ++metrics_.edge_count;
            return true;
        }

        [[nodiscard]] bool emit(const Signal& signal)
        {
            if (!valid_signal(signal)
                || signal.timestamp_ns < metrics_.latest_timestamp_ns)
            {
                ++metrics_.rejected_signal_count;
                return false;
            }

            if (pending_count() >= limits_.maximum_pending_activations)
            {
                ++metrics_.rejected_signal_count;
                ++metrics_.dropped_activation_count;
                return false;
            }

            metrics_.latest_timestamp_ns = signal.timestamp_ns;
            ++metrics_.emitted_signal_count;

            if (signal.target)
            {
                if (!node_record(signal.target))
                {
                    ++metrics_.rejected_signal_count;
                    return false;
                }
                return queue_activation(signal.target, signal, 1.0f);
            }

            const std::size_t index = route_index(signal.channel);
            if (index == routes_.size())
                return true;

            const auto& targets = routes_[index].targets;
            if (pending_count() + targets.size() > limits_.maximum_pending_activations)
            {
                metrics_.dropped_activation_count += targets.size();
                return false;
            }

            for (const NodeHandle target : targets)
            {
                if (!queue_activation(target, signal, 1.0f))
                    return false;
            }
            return true;
        }

        [[nodiscard]] PumpResult pump(
            TimestampNanoseconds through_timestamp_ns,
            std::size_t activation_budget,
            SpikeSink sink = nullptr,
            void* sink_user = nullptr) noexcept
        {
            PumpResult result{};
            if (activation_budget == 0u)
            {
                result.pending_activations = pending_count();
                result.budget_exhausted = result.pending_activations > 0u;
                return result;
            }

            while (pending_head_ < pending_.size()
                && result.processed_activations < activation_budget)
            {
                const PendingActivation activation = pending_[pending_head_];
                if (activation.signal.timestamp_ns > through_timestamp_ns)
                    break;

                ++pending_head_;
                ++result.processed_activations;
                ++metrics_.processed_activation_count;

                NodeRecord* node = node_record(activation.target);
                if (!node)
                    continue;

                ++node->activation_count;
                integrate(*node, activation.signal.timestamp_ns);

                float signedMagnitude = activation.signal.magnitude;
                if (activation.signal.polarity == Polarity::inhibitory)
                    signedMagnitude = -signedMagnitude;
                else if (activation.signal.polarity == Polarity::neutral)
                    signedMagnitude = 0.0f;

                const float salience = std::clamp(
                    activation.signal.salience,
                    0.0f,
                    4.0f);
                node->potential += signedMagnitude
                    * salience
                    * node->config.gain
                    * activation.weight;

                if (node->potential < node->config.threshold)
                    continue;

                if (node->last_fire_ns != 0u
                    && activation.signal.timestamp_ns >= node->last_fire_ns
                    && activation.signal.timestamp_ns - node->last_fire_ns
                        < node->config.refractory_ns)
                {
                    ++metrics_.suppressed_refractory_count;
                    continue;
                }

                node->last_fire_ns = activation.signal.timestamp_ns;
                ++node->fire_count;
                ++metrics_.fired_node_count;
                ++result.fired_nodes;

                Signal spike = activation.signal;
                spike.target = {};
                spike.channel = node->config.output_channel;
                spike.semantic = node->config.semantic != 0u
                    ? node->config.semantic
                    : activation.signal.semantic;
                spike.source = encode_handle(activation.target);
                spike.magnitude = (std::max)(
                    node->config.threshold,
                    std::abs(node->potential));
                spike.polarity = Polarity::excitatory;

                node->potential = node->config.reset_potential;

                if (sink)
                    sink(sink_user, spike);

                for (const OutgoingEdge& edge : node->outgoing)
                {
                    if (pending_count() >= limits_.maximum_pending_activations)
                    {
                        ++metrics_.dropped_activation_count;
                        continue;
                    }

                    Signal propagated = spike;
                    propagated.target = edge.target;
                    propagated.polarity = edge.weight < 0.0f
                        ? Polarity::inhibitory
                        : Polarity::excitatory;
                    if (queue_activation(edge.target, propagated, std::abs(edge.weight)))
                        ++result.propagated_activations;
                }
            }

            compact_pending_if_needed();
            metrics_.pending_count = pending_count();
            result.pending_activations = metrics_.pending_count;
            result.budget_exhausted = result.processed_activations >= activation_budget
                && result.pending_activations > 0u;
            return result;
        }

        [[nodiscard]] NodeSnapshot snapshot(NodeHandle handle) const noexcept
        {
            const NodeRecord* node = node_record(handle);
            if (!node)
                return {};

            return NodeSnapshot{
                .handle = handle,
                .config = node->config,
                .potential = node->potential,
                .last_update_ns = node->last_update_ns,
                .last_fire_ns = node->last_fire_ns,
                .activation_count = node->activation_count,
                .fire_count = node->fire_count,
                .active = true
            };
        }

        [[nodiscard]] GraphMetrics metrics() const noexcept
        {
            GraphMetrics result = metrics_;
            result.pending_count = pending_count();
            return result;
        }

        void reset(TimestampNanoseconds timestamp_ns = 0u) noexcept
        {
            for (NodeRecord& node : nodes_)
            {
                node.potential = node.config.reset_potential;
                node.last_update_ns = timestamp_ns;
                node.last_fire_ns = 0u;
                node.activation_count = 0u;
                node.fire_count = 0u;
            }
            pending_.clear();
            pending_head_ = 0u;

            metrics_.pending_count = 0u;
            metrics_.emitted_signal_count = 0u;
            metrics_.rejected_signal_count = 0u;
            metrics_.routed_activation_count = 0u;
            metrics_.processed_activation_count = 0u;
            metrics_.fired_node_count = 0u;
            metrics_.suppressed_refractory_count = 0u;
            metrics_.dropped_activation_count = 0u;
            metrics_.latest_timestamp_ns = timestamp_ns;
        }

    private:
        struct OutgoingEdge final
        {
            NodeHandle target{};
            float weight{1.0f};
        };

        struct NodeRecord final
        {
            std::uint32_t generation{1u};
            bool active{};
            NodeConfig config{};
            float potential{};
            TimestampNanoseconds last_update_ns{};
            TimestampNanoseconds last_fire_ns{};
            std::uint64_t activation_count{};
            std::uint64_t fire_count{};
            std::vector<OutgoingEdge> outgoing{};
        };

        struct ChannelRoute final
        {
            ChannelId channel{};
            std::vector<NodeHandle> targets{};
        };

        struct PendingActivation final
        {
            NodeHandle target{};
            Signal signal{};
            float weight{1.0f};
        };

        [[nodiscard]] static constexpr std::uint64_t encode_handle(
            NodeHandle handle) noexcept
        {
            return (static_cast<std::uint64_t>(handle.generation) << 32u)
                | static_cast<std::uint64_t>(handle.index);
        }

        [[nodiscard]] static bool valid_config(const NodeConfig& config) noexcept
        {
            return config.input_channel != 0u
                && config.output_channel != 0u
                && std::isfinite(config.threshold)
                && config.threshold > 0.0f
                && std::isfinite(config.gain)
                && std::abs(config.gain) <= 64.0f
                && std::isfinite(config.leak_per_second)
                && config.leak_per_second >= 0.0f
                && config.leak_per_second <= 1'000'000.0f
                && std::isfinite(config.reset_potential);
        }

        [[nodiscard]] static bool valid_signal(const Signal& signal) noexcept
        {
            return (signal.target || signal.channel != 0u)
                && std::isfinite(signal.magnitude)
                && signal.magnitude >= 0.0f
                && std::isfinite(signal.salience)
                && signal.salience >= 0.0f
                && signal.salience <= 16.0f;
        }

        [[nodiscard]] NodeRecord* node_record(NodeHandle handle) noexcept
        {
            if (!handle || handle.index >= nodes_.size())
                return nullptr;
            NodeRecord& record = nodes_[handle.index];
            if (!record.active || record.generation != handle.generation)
                return nullptr;
            return &record;
        }

        [[nodiscard]] const NodeRecord* node_record(NodeHandle handle) const noexcept
        {
            if (!handle || handle.index >= nodes_.size())
                return nullptr;
            const NodeRecord& record = nodes_[handle.index];
            if (!record.active || record.generation != handle.generation)
                return nullptr;
            return &record;
        }

        [[nodiscard]] std::size_t route_index(ChannelId channel) const noexcept
        {
            const auto it = std::lower_bound(
                routes_.begin(),
                routes_.end(),
                channel,
                [](const ChannelRoute& route, ChannelId value)
                {
                    return route.channel < value;
                });
            if (it == routes_.end() || it->channel != channel)
                return routes_.size();
            return static_cast<std::size_t>(std::distance(routes_.begin(), it));
        }

        [[nodiscard]] bool add_route(ChannelId channel, NodeHandle target)
        {
            auto it = std::lower_bound(
                routes_.begin(),
                routes_.end(),
                channel,
                [](const ChannelRoute& route, ChannelId value)
                {
                    return route.channel < value;
                });
            if (it == routes_.end() || it->channel != channel)
            {
                it = routes_.insert(it, ChannelRoute{channel, {}});
                it->targets.reserve(4u);
            }
            it->targets.push_back(target);
            return true;
        }

        [[nodiscard]] bool queue_activation(
            NodeHandle target,
            const Signal& signal,
            float weight)
        {
            if (pending_count() >= limits_.maximum_pending_activations)
            {
                ++metrics_.dropped_activation_count;
                return false;
            }
            pending_.push_back(PendingActivation{target, signal, weight});
            ++metrics_.routed_activation_count;
            metrics_.pending_count = pending_count();
            return true;
        }

        [[nodiscard]] std::size_t pending_count() const noexcept
        {
            return pending_.size() - pending_head_;
        }

        void compact_pending_if_needed()
        {
            if (pending_head_ == pending_.size())
            {
                pending_.clear();
                pending_head_ = 0u;
                return;
            }

            if (pending_head_ >= 4'096u && pending_head_ * 2u >= pending_.size())
            {
                pending_.erase(
                    pending_.begin(),
                    pending_.begin() + static_cast<std::ptrdiff_t>(pending_head_));
                pending_head_ = 0u;
            }
        }

        static void integrate(
            NodeRecord& node,
            TimestampNanoseconds timestamp_ns) noexcept
        {
            if (node.last_update_ns == 0u || timestamp_ns <= node.last_update_ns)
            {
                node.last_update_ns = timestamp_ns;
                return;
            }

            const double elapsedSeconds = static_cast<double>(
                timestamp_ns - node.last_update_ns) / 1'000'000'000.0;
            const double retained = (std::max)(
                0.0,
                1.0 - static_cast<double>(node.config.leak_per_second) * elapsedSeconds);
            node.potential = static_cast<float>(
                static_cast<double>(node.potential) * retained);
            node.last_update_ns = timestamp_ns;
        }

        GraphLimits limits_{};
        std::vector<NodeRecord> nodes_{};
        std::vector<ChannelRoute> routes_{};
        std::vector<PendingActivation> pending_{};
        std::size_t pending_head_{};
        GraphMetrics metrics_{};
    };

    struct ContractChecks final
    {
        bool sparse_routing{};
        bool threshold_accumulation{};
        bool graph_propagation{};
        bool refractory_gate{};
        bool inhibitory_signal{};
        bool deterministic_reset{};

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return sparse_routing
                && threshold_accumulation
                && graph_propagation
                && refractory_gate
                && inhibitory_signal
                && deterministic_reset;
        }
    };

    [[nodiscard]] ContractChecks run_contract_checks()
    {
        struct SinkState final
        {
            std::size_t count{};
            Signal last{};
        } sinkState{};

        const auto sink = [](void* user, const Signal& signal) noexcept
        {
            auto& state = *static_cast<SinkState*>(user);
            ++state.count;
            state.last = signal;
        };

        Graph graph{};
        const NodeHandle sensory = graph.add_node(NodeConfig{
            .input_channel = channels::event_camera,
            .output_channel = combine_id(channels::event_camera, 1u),
            .semantic = stable_id("contract.sensory"),
            .threshold = 1.0f,
            .gain = 1.0f,
            .leak_per_second = 0.0f,
            .reset_potential = 0.0f,
            .refractory_ns = 1'000u
        });
        const NodeHandle attention = graph.add_node(NodeConfig{
            .input_channel = stable_id("contract.unused.route"),
            .output_channel = stable_id("contract.attention"),
            .semantic = stable_id("contract.attention"),
            .threshold = 0.5f,
            .gain = 1.0f,
            .leak_per_second = 0.0f,
            .reset_potential = 0.0f,
            .refractory_ns = 1'000u
        });
        const NodeHandle idle = graph.add_node(NodeConfig{
            .input_channel = stable_id("contract.idle"),
            .output_channel = stable_id("contract.idle.out"),
            .threshold = 1.0f
        });
        const bool connected = graph.connect(sensory, attention, 1.0f);

        Signal half{};
        half.channel = channels::event_camera;
        half.timestamp_ns = 10'000u;
        half.magnitude = 0.5f;
        half.salience = 1.0f;
        half.kind = SignalKind::sensory;
        half.truth = TruthClass::visual_only;
        half.polarity = Polarity::excitatory;

        const bool emittedFirst = graph.emit(half);
        const PumpResult firstPump = graph.pump(10'000u, 32u, sink, &sinkState);
        const auto sensoryAfterFirst = graph.snapshot(sensory);
        const auto idleAfterFirst = graph.snapshot(idle);

        half.timestamp_ns = 12'000u;
        const bool emittedSecond = graph.emit(half);
        const PumpResult secondPump = graph.pump(12'000u, 32u, sink, &sinkState);
        const auto sensoryAfterSecond = graph.snapshot(sensory);
        const auto attentionAfterSecond = graph.snapshot(attention);

        Signal refractoryProbe = half;
        refractoryProbe.timestamp_ns = 12'100u;
        refractoryProbe.magnitude = 1.0f;
        const bool emittedRefractory = graph.emit(refractoryProbe);
        const PumpResult refractoryPump = graph.pump(12'100u, 32u, sink, &sinkState);
        const auto sensoryAfterRefractory = graph.snapshot(sensory);

        Graph inhibitionGraph{};
        const NodeHandle inhibitionNode = inhibitionGraph.add_node(NodeConfig{
            .input_channel = channels::simulation,
            .output_channel = stable_id("contract.inhibition.out"),
            .threshold = 1.0f,
            .gain = 1.0f,
            .leak_per_second = 0.0f,
            .reset_potential = 0.0f,
            .refractory_ns = 0u
        });
        Signal positive{};
        positive.channel = channels::simulation;
        positive.timestamp_ns = 1'000u;
        positive.magnitude = 0.75f;
        positive.polarity = Polarity::excitatory;
        const bool positiveAccepted = inhibitionGraph.emit(positive);
        static_cast<void>(inhibitionGraph.pump(1'000u, 8u));
        Signal negative = positive;
        negative.timestamp_ns = 2'000u;
        negative.magnitude = 0.5f;
        negative.polarity = Polarity::inhibitory;
        const bool negativeAccepted = inhibitionGraph.emit(negative);
        static_cast<void>(inhibitionGraph.pump(2'000u, 8u));
        const auto inhibitionSnapshot = inhibitionGraph.snapshot(inhibitionNode);

        const GraphMetrics beforeReset = graph.metrics();
        graph.reset(50'000u);
        const GraphMetrics afterReset = graph.metrics();
        const auto sensoryAfterReset = graph.snapshot(sensory);

        return ContractChecks{
            .sparse_routing = emittedFirst
                && firstPump.processed_activations == 1u
                && sensoryAfterFirst.activation_count == 1u
                && idleAfterFirst.activation_count == 0u,
            .threshold_accumulation = emittedSecond
                && sensoryAfterSecond.fire_count == 1u
                && secondPump.fired_nodes >= 2u,
            .graph_propagation = connected
                && attentionAfterSecond.activation_count == 1u
                && attentionAfterSecond.fire_count == 1u
                && sinkState.count >= 2u,
            .refractory_gate = emittedRefractory
                && refractoryPump.processed_activations >= 1u
                && sensoryAfterRefractory.fire_count == 1u
                && beforeReset.suppressed_refractory_count >= 1u,
            .inhibitory_signal = positiveAccepted
                && negativeAccepted
                && inhibitionSnapshot.fire_count == 0u
                && inhibitionSnapshot.potential > 0.20f
                && inhibitionSnapshot.potential < 0.30f,
            .deterministic_reset = afterReset.emitted_signal_count == 0u
                && afterReset.processed_activation_count == 0u
                && afterReset.latest_timestamp_ns == 50'000u
                && sensoryAfterReset.fire_count == 0u
                && sensoryAfterReset.activation_count == 0u
        };
    }
}
