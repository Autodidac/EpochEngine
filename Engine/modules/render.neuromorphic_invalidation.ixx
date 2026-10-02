/************************************************
 * Epoch Engine - Neuromorphic Render Invalidation
 *
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 * Provided "AS IS", without warranty of any kind.
 * See LICENSE for full terms.
 ***********************************************/
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

export module render.neuromorphic_invalidation;

import simulation.neuromorphic;

export namespace epochengine::render::neuromorphic_invalidation
{
    namespace neuro = epochengine::simulation::neuromorphic;

    inline constexpr neuro::ChannelId kSceneChangeChannel =
        neuro::stable_id("epoch.render.scene_change");
    inline constexpr neuro::ChannelId kSceneActivityChannel =
        neuro::stable_id("epoch.render.scene_activity");
    inline constexpr neuro::ChannelId kDenseActivityChannel =
        neuro::stable_id("epoch.render.dense_activity");

    struct ChangeSample final
    {
        std::uint64_t source{};
        float magnitude{1.0f};
        float salience{1.0f};
    };

    struct Decision final
    {
        bool valid{};
        bool force_full_redraw{};
        bool budget_exhausted{};
        std::size_t sample_count{};
        std::size_t processed_activations{};
        std::size_t fired_nodes{};
        std::size_t dense_fires{};
        std::uint64_t dropped_activations{};
    };

    // A tiny persistent weighted network used only to decide when sparse scene
    // invalidation has become dense enough that a full redraw is cheaper/safer.
    // It may promote partial -> full rendering, but it never suppresses a real
    // invalidation, so renderer correctness does not depend on the heuristic.
    class InvalidationNetwork final
    {
    public:
        InvalidationNetwork()
            : graph_(neuro::GraphLimits{
                .maximum_nodes = 8u,
                .maximum_edges = 16u,
                .maximum_pending_activations = 512u,
                .maximum_routes = 8u
            })
        {
            activity_ = graph_.add_node(neuro::NodeConfig{
                .input_channel = kSceneChangeChannel,
                .output_channel = kSceneActivityChannel,
                .semantic = neuro::stable_id("render.scene.activity"),
                .threshold = 0.70f,
                .gain = 1.0f,
                .leak_per_second = 6.0f,
                .reset_potential = 0.0f,
                .refractory_ns = 0u
            });

            density_ = graph_.add_node(neuro::NodeConfig{
                .input_channel = neuro::stable_id("epoch.render.scene_activity.direct"),
                .output_channel = kDenseActivityChannel,
                .semantic = neuro::stable_id("render.scene.dense_activity"),
                .threshold = 3.0f,
                .gain = 1.0f,
                .leak_per_second = 3.0f,
                .reset_potential = 0.0f,
                .refractory_ns = 250'000u
            });

            valid_ = activity_ && density_ && graph_.connect(activity_, density_, 0.72f);
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return valid_;
        }

        void reset(neuro::TimestampNanoseconds timestamp_ns = 0u) noexcept
        {
            graph_.reset(timestamp_ns);
            sequence_ = 0u;
        }

        [[nodiscard]] Decision evaluate(
            neuro::TimestampNanoseconds timestamp_ns,
            std::span<const ChangeSample> samples,
            bool global_invalidation = false) noexcept
        {
            Decision decision{};
            decision.valid = valid_;
            decision.sample_count = samples.size();
            if (!valid_)
            {
                decision.force_full_redraw = global_invalidation;
                return decision;
            }

            const auto before = graph_.metrics();
            if (timestamp_ns < before.latest_timestamp_ns)
                graph_.reset(timestamp_ns);

            if (global_invalidation)
            {
                graph_.reset(timestamp_ns);
                decision.force_full_redraw = true;
                return decision;
            }

            for (const ChangeSample& sample : samples)
            {
                neuro::Signal signal{};
                signal.channel = kSceneChangeChannel;
                signal.semantic = neuro::stable_id("render.scene.change");
                signal.source = sample.source;
                signal.sequence = ++sequence_;
                signal.timestamp_ns = timestamp_ns;
                signal.magnitude = (std::clamp)(sample.magnitude, 0.0f, 4.0f);
                signal.salience = (std::clamp)(sample.salience, 0.0f, 4.0f);
                signal.kind = neuro::SignalKind::sensory;
                signal.truth = neuro::TruthClass::visual_only;
                signal.polarity = neuro::Polarity::excitatory;
                (void)graph_.emit(signal);
            }

            struct Sink final
            {
                std::size_t dense_fires{};
            } sinkState{};

            const auto sink = [](void* user, const neuro::Signal& signal) noexcept
            {
                auto& sink = *static_cast<Sink*>(user);
                if (signal.channel == kDenseActivityChannel)
                    ++sink.dense_fires;
            };

            const std::size_t budget = (std::max<std::size_t>)(
                32u,
                samples.size() * 4u + 16u);
            const neuro::PumpResult pump = graph_.pump(
                timestamp_ns,
                budget,
                sink,
                &sinkState);
            const auto after = graph_.metrics();

            decision.processed_activations = pump.processed_activations;
            decision.fired_nodes = pump.fired_nodes;
            decision.dense_fires = sinkState.dense_fires;
            decision.budget_exhausted = pump.budget_exhausted;
            decision.dropped_activations = after.dropped_activation_count;
            decision.force_full_redraw = sinkState.dense_fires > 0u
                || pump.budget_exhausted
                || after.dropped_activation_count > before.dropped_activation_count;
            return decision;
        }

    private:
        neuro::Graph graph_{};
        neuro::NodeHandle activity_{};
        neuro::NodeHandle density_{};
        std::uint64_t sequence_{};
        bool valid_{};
    };

    struct ContractChecks final
    {
        bool sparse_change_stays_partial{};
        bool dense_change_promotes_full{};
        bool global_invalidation_promotes_full{};
        bool reset_recovers_sparse_path{};

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return sparse_change_stays_partial
                && dense_change_promotes_full
                && global_invalidation_promotes_full
                && reset_recovers_sparse_path;
        }
    };

    [[nodiscard]] ContractChecks run_contract_checks() noexcept
    {
        InvalidationNetwork network{};
        const ChangeSample sparse[]{{.source = 1u, .magnitude = 0.8f, .salience = 1.0f}};
        const Decision sparseDecision = network.evaluate(1'000'000u, sparse);

        ChangeSample dense[12]{};
        for (std::size_t i = 0; i < 12u; ++i)
        {
            dense[i].source = 100u + i;
            dense[i].magnitude = 1.0f;
            dense[i].salience = 1.0f;
        }
        const Decision denseDecision = network.evaluate(2'000'000u, dense);
        const Decision globalDecision = network.evaluate(3'000'000u, {}, true);

        network.reset(4'000'000u);
        const Decision sparseAfterReset = network.evaluate(5'000'000u, sparse);

        return ContractChecks{
            .sparse_change_stays_partial = network.valid()
                && sparseDecision.valid
                && !sparseDecision.force_full_redraw,
            .dense_change_promotes_full = denseDecision.force_full_redraw
                && denseDecision.dense_fires > 0u,
            .global_invalidation_promotes_full = globalDecision.force_full_redraw,
            .reset_recovers_sparse_path = !sparseAfterReset.force_full_redraw
        };
    }
}
