/************************************************
 * Epoch Engine - Neuromorphic Camera
 *
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 * Provided "AS IS", without warranty of any kind.
 * See LICENSE for full terms.
 ***********************************************/
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

export module render.neuromorphic_camera;

import simulation.neuromorphic;

export namespace epochengine::render::neuromorphic_camera
{
    using epochengine::simulation::neuromorphic::ChannelId;
    using epochengine::simulation::neuromorphic::Polarity;
    using epochengine::simulation::neuromorphic::Signal;
    using epochengine::simulation::neuromorphic::SignalKind;
    using epochengine::simulation::neuromorphic::TimestampNanoseconds;
    using epochengine::simulation::neuromorphic::TruthClass;

    struct Config final
    {
        std::uint32_t width{};
        std::uint32_t height{};
        float positive_log_threshold{0.20f};
        float negative_log_threshold{0.20f};
        float minimum_luminance{1.0e-4f};
        TimestampNanoseconds pixel_refractory_ns{0u};
        std::uint32_t maximum_events_per_pixel_per_observation{8u};

        [[nodiscard]] bool valid() const noexcept
        {
            const std::uint64_t pixels = static_cast<std::uint64_t>(width)
                * static_cast<std::uint64_t>(height);
            return width > 0u
                && height > 0u
                && pixels <= 16'777'216ull
                && std::isfinite(positive_log_threshold)
                && positive_log_threshold > 0.0f
                && std::isfinite(negative_log_threshold)
                && negative_log_threshold > 0.0f
                && std::isfinite(minimum_luminance)
                && minimum_luminance > 0.0f
                && maximum_events_per_pixel_per_observation > 0u
                && maximum_events_per_pixel_per_observation <= 64u;
        }
    };

    struct PixelEvent final
    {
        std::uint32_t x{};
        std::uint32_t y{};
        TimestampNanoseconds timestamp_ns{};
        float log_contrast{};
        Polarity polarity{Polarity::neutral};
    };

    using EventSink = void (*)(void* user, const PixelEvent& event) noexcept;

    struct ObservationMetrics final
    {
        std::size_t event_count{};
        std::size_t positive_event_count{};
        std::size_t negative_event_count{};
        std::size_t refractory_suppressed_count{};
        std::size_t invalid_sample_count{};
        double activity_centroid_x{};
        double activity_centroid_y{};
        double normalized_activity{};
        bool reference_initialized{};
    };

    class EventCamera final
    {
    public:
        explicit EventCamera(Config config)
            : config_(config)
        {
            if (!config_.valid())
                return;

            const std::size_t pixelCount = static_cast<std::size_t>(config_.width)
                * static_cast<std::size_t>(config_.height);
            reference_log_luminance_.resize(pixelCount, 0.0f);
            last_event_ns_.resize(pixelCount, 0u);
            valid_ = true;
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return valid_;
        }

        [[nodiscard]] const Config& config() const noexcept
        {
            return config_;
        }

        void reset() noexcept
        {
            std::fill(reference_log_luminance_.begin(), reference_log_luminance_.end(), 0.0f);
            std::fill(last_event_ns_.begin(), last_event_ns_.end(), 0u);
            initialized_ = false;
            last_observation_ns_ = 0u;
        }

        [[nodiscard]] ObservationMetrics observe(
            TimestampNanoseconds timestamp_ns,
            std::span<const float> luminance,
            EventSink sink = nullptr,
            void* sink_user = nullptr) noexcept
        {
            ObservationMetrics metrics{};
            if (!valid_
                || timestamp_ns < last_observation_ns_
                || luminance.size() != reference_log_luminance_.size())
            {
                return metrics;
            }

            last_observation_ns_ = timestamp_ns;

            if (!initialized_)
            {
                for (std::size_t index = 0u; index < luminance.size(); ++index)
                {
                    reference_log_luminance_[index] = sample_log_luminance(
                        luminance[index],
                        metrics.invalid_sample_count);
                }
                initialized_ = true;
                metrics.reference_initialized = true;
                return metrics;
            }

            double weightedX = 0.0;
            double weightedY = 0.0;
            for (std::size_t index = 0u; index < luminance.size(); ++index)
            {
                const float currentLog = sample_log_luminance(
                    luminance[index],
                    metrics.invalid_sample_count);
                float delta = currentLog - reference_log_luminance_[index];
                if (!std::isfinite(delta))
                    continue;

                const bool positive = delta >= config_.positive_log_threshold;
                const bool negative = delta <= -config_.negative_log_threshold;
                if (!positive && !negative)
                    continue;

                if (config_.pixel_refractory_ns > 0u
                    && last_event_ns_[index] != 0u
                    && timestamp_ns >= last_event_ns_[index]
                    && timestamp_ns - last_event_ns_[index] < config_.pixel_refractory_ns)
                {
                    ++metrics.refractory_suppressed_count;
                    continue;
                }

                const Polarity polarity = positive
                    ? Polarity::excitatory
                    : Polarity::inhibitory;
                const float threshold = positive
                    ? config_.positive_log_threshold
                    : config_.negative_log_threshold;
                const std::uint32_t x = static_cast<std::uint32_t>(
                    index % static_cast<std::size_t>(config_.width));
                const std::uint32_t y = static_cast<std::uint32_t>(
                    index / static_cast<std::size_t>(config_.width));

                std::uint32_t emittedForPixel = 0u;
                while (std::abs(delta) >= threshold
                    && emittedForPixel < config_.maximum_events_per_pixel_per_observation)
                {
                    const float signedThreshold = positive ? threshold : -threshold;
                    PixelEvent event{
                        .x = x,
                        .y = y,
                        .timestamp_ns = timestamp_ns,
                        .log_contrast = signedThreshold,
                        .polarity = polarity
                    };
                    if (sink)
                        sink(sink_user, event);

                    ++metrics.event_count;
                    if (positive)
                        ++metrics.positive_event_count;
                    else
                        ++metrics.negative_event_count;
                    weightedX += static_cast<double>(x);
                    weightedY += static_cast<double>(y);

                    reference_log_luminance_[index] += signedThreshold;
                    delta = currentLog - reference_log_luminance_[index];
                    ++emittedForPixel;
                }

                if (emittedForPixel > 0u)
                    last_event_ns_[index] = timestamp_ns;
            }

            if (metrics.event_count > 0u)
            {
                metrics.activity_centroid_x = weightedX
                    / static_cast<double>(metrics.event_count);
                metrics.activity_centroid_y = weightedY
                    / static_cast<double>(metrics.event_count);
                metrics.normalized_activity = static_cast<double>(metrics.event_count)
                    / static_cast<double>(reference_log_luminance_.size());
            }
            metrics.reference_initialized = true;
            return metrics;
        }

    private:
        [[nodiscard]] float sample_log_luminance(
            float luminance,
            std::size_t& invalidCount) const noexcept
        {
            if (!std::isfinite(luminance) || luminance < 0.0f)
            {
                ++invalidCount;
                luminance = config_.minimum_luminance;
            }
            return std::log((std::max)(luminance, config_.minimum_luminance));
        }

        Config config_{};
        std::vector<float> reference_log_luminance_{};
        std::vector<TimestampNanoseconds> last_event_ns_{};
        TimestampNanoseconds last_observation_ns_{};
        bool initialized_{};
        bool valid_{};
    };

    [[nodiscard]] constexpr std::uint64_t pixel_source_id(
        std::uint32_t x,
        std::uint32_t y) noexcept
    {
        return (static_cast<std::uint64_t>(y) << 32u)
            | static_cast<std::uint64_t>(x);
    }

    [[nodiscard]] inline Signal to_signal(
        const PixelEvent& event,
        ChannelId channel = epochengine::simulation::neuromorphic::channels::event_camera,
        std::uint64_t sequence = 0u) noexcept
    {
        Signal signal{};
        signal.channel = channel != 0u
            ? channel
            : epochengine::simulation::neuromorphic::channels::event_camera;
        signal.semantic = epochengine::simulation::neuromorphic::stable_id(
            "render.neuromorphic_camera.pixel_change");
        signal.source = pixel_source_id(event.x, event.y);
        signal.sequence = sequence;
        signal.timestamp_ns = event.timestamp_ns;
        signal.magnitude = std::abs(event.log_contrast);
        signal.salience = (std::min)(4.0f, 1.0f + std::abs(event.log_contrast));
        signal.kind = SignalKind::sensory;
        signal.truth = TruthClass::visual_only;
        signal.polarity = event.polarity;
        return signal;
    }

    struct ContractChecks final
    {
        bool initializes_without_events{};
        bool positive_and_negative_events{};
        bool unchanged_scene_is_sparse{};
        bool activity_centroid{};
        bool signal_bridge{};

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return initializes_without_events
                && positive_and_negative_events
                && unchanged_scene_is_sparse
                && activity_centroid
                && signal_bridge;
        }
    };

    [[nodiscard]] ContractChecks run_contract_checks()
    {
        struct SinkState final
        {
            std::array<PixelEvent, 64u> events{};
            std::size_t count{};
        } sinkState{};

        const auto sink = [](void* user, const PixelEvent& event) noexcept
        {
            auto& state = *static_cast<SinkState*>(user);
            if (state.count < state.events.size())
                state.events[state.count++] = event;
        };

        EventCamera camera(Config{
            .width = 2u,
            .height = 2u,
            .positive_log_threshold = 0.20f,
            .negative_log_threshold = 0.20f,
            .minimum_luminance = 1.0e-4f,
            .pixel_refractory_ns = 0u,
            .maximum_events_per_pixel_per_observation = 8u
        });

        const float initialValues[] = {0.5f, 0.5f, 0.5f, 0.5f};
        const float changedValues[] = {1.0f, 0.5f, 0.5f, 0.25f};
        const ObservationMetrics initial = camera.observe(
            1'000u,
            std::span<const float>{initialValues},
            sink,
            &sinkState);
        const ObservationMetrics changed = camera.observe(
            2'000u,
            std::span<const float>{changedValues},
            sink,
            &sinkState);
        const std::size_t afterChanged = sinkState.count;
        const ObservationMetrics unchanged = camera.observe(
            3'000u,
            std::span<const float>{changedValues},
            sink,
            &sinkState);

        bool bridgePassed = false;
        if (sinkState.count > 0u)
        {
            const Signal signal = to_signal(sinkState.events[0], {}, 7u);
            bridgePassed = signal.channel
                    == epochengine::simulation::neuromorphic::channels::event_camera
                && signal.sequence == 7u
                && signal.timestamp_ns == sinkState.events[0].timestamp_ns
                && signal.kind == SignalKind::sensory
                && signal.truth == TruthClass::visual_only
                && signal.polarity == sinkState.events[0].polarity;
        }

        return ContractChecks{
            .initializes_without_events = camera.valid()
                && initial.reference_initialized
                && initial.event_count == 0u,
            .positive_and_negative_events = changed.event_count >= 2u
                && changed.positive_event_count > 0u
                && changed.negative_event_count > 0u,
            .unchanged_scene_is_sparse = afterChanged > 0u
                && unchanged.event_count == 0u
                && sinkState.count == afterChanged,
            .activity_centroid = changed.activity_centroid_x >= 0.0
                && changed.activity_centroid_x <= 1.0
                && changed.activity_centroid_y >= 0.0
                && changed.activity_centroid_y <= 1.0
                && changed.normalized_activity > 0.0,
            .signal_bridge = bridgePassed
        };
    }
}
