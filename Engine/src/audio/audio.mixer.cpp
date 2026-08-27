/*
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

module audio.mixer;

namespace epochengine::audio
{
    namespace
    {
        constexpr std::uint32_t minimum_sample_rate = 8'000;
        constexpr std::uint32_t maximum_sample_rate = 768'000;
        constexpr std::uint32_t hard_maximum_clips = 65'536;
        constexpr std::uint32_t hard_maximum_bus_bindings = 4'096;
        constexpr std::uint32_t hard_maximum_sources_per_mix = 65'536;
        constexpr std::uint64_t hard_maximum_frames_per_clip = 1ull << 32;
        constexpr std::uint64_t hard_maximum_resident_samples = 1ull << 34;
        constexpr std::uint32_t hard_maximum_output_frames = 7'680'000;
        constexpr std::uint64_t fnv_offset = 14'695'981'039'346'656'037ull;
        constexpr std::uint64_t fnv_prime = 1'099'511'628'211ull;

        [[nodiscard]] constexpr std::uint32_t channel_count(
            PcmChannelLayout layout) noexcept
        {
            switch (layout)
            {
            case PcmChannelLayout::mono:
                return 1;
            case PcmChannelLayout::stereo:
                return 2;
            case PcmChannelLayout::surround_5_1:
                return 6;
            }
            return 0;
        }

        [[nodiscard]] constexpr bool supported_clip_format(
            const PcmFormat& format) noexcept
        {
            return format.sample_format
                    == PcmSampleFormat::float32_interleaved
                && (format.channel_layout == PcmChannelLayout::mono
                    || format.channel_layout == PcmChannelLayout::stereo)
                && format.sample_rate >= minimum_sample_rate
                && format.sample_rate <= maximum_sample_rate;
        }

        [[nodiscard]] constexpr bool supported_output_format(
            const AudioOutputFormat& format) noexcept
        {
            return format.sample_format
                    == PcmSampleFormat::float32_interleaved
                && format.channel_layout == PcmChannelLayout::stereo
                && format.sample_rate >= minimum_sample_rate
                && format.sample_rate <= maximum_sample_rate;
        }

        [[nodiscard]] constexpr AudioMixerLimits sanitize_limits(
            AudioMixerLimits limits) noexcept
        {
            limits.maximum_clips = (std::clamp)(
                limits.maximum_clips,
                1u,
                hard_maximum_clips);
            limits.maximum_bus_bindings = (std::clamp)(
                limits.maximum_bus_bindings,
                1u,
                hard_maximum_bus_bindings);
            limits.maximum_sources_per_mix = (std::clamp)(
                limits.maximum_sources_per_mix,
                1u,
                hard_maximum_sources_per_mix);
            limits.maximum_frames_per_clip = (std::clamp)(
                limits.maximum_frames_per_clip,
                std::uint64_t{ 1 },
                hard_maximum_frames_per_clip);
            limits.maximum_resident_samples = (std::clamp)(
                limits.maximum_resident_samples,
                std::uint64_t{ 1 },
                hard_maximum_resident_samples);
            limits.maximum_output_frames_per_mix = (std::clamp)(
                limits.maximum_output_frames_per_mix,
                1u,
                hard_maximum_output_frames);
            return limits;
        }

        template <class Value>
        void saturating_add(Value& destination, Value increment) noexcept
        {
            const Value maximum = (std::numeric_limits<Value>::max)();
            destination = increment > maximum - destination
                ? maximum
                : static_cast<Value>(destination + increment);
        }

        [[nodiscard]] constexpr bool multiply_fits(
            std::uint64_t lhs,
            std::uint64_t rhs) noexcept
        {
            return lhs == 0
                || rhs <= (std::numeric_limits<std::uint64_t>::max)() / lhs;
        }

        void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept
        {
            hash ^= value;
            hash *= fnv_prime;
        }

        template <class Integer>
        void hash_integer(std::uint64_t& hash, Integer value) noexcept
        {
            using Unsigned = std::make_unsigned_t<Integer>;
            const Unsigned bits = static_cast<Unsigned>(value);
            for (std::size_t index = 0; index < sizeof(Unsigned); ++index)
            {
                hash_byte(
                    hash,
                    static_cast<std::uint8_t>(bits >> (index * 8u)));
            }
        }

        [[nodiscard]] std::uint64_t clip_digest(
            const OwnedPcmClip& clip) noexcept
        {
            std::uint64_t hash = fnv_offset;
            hash_integer(hash, clip.id.domain);
            hash_integer(hash, clip.id.value);
            hash_integer(hash, static_cast<std::uint8_t>(clip.format.sample_format));
            hash_integer(hash, static_cast<std::uint8_t>(clip.format.channel_layout));
            hash_integer(hash, clip.format.sample_rate);
            hash_integer(hash, clip.frame_count);
            hash_integer(hash, static_cast<std::uint64_t>(clip.interleaved_samples.size()));
            for (float sample : clip.interleaved_samples)
                hash_integer(hash, std::bit_cast<std::uint32_t>(sample));
            return hash;
        }

        [[nodiscard]] constexpr bool logical_id_less(
            LogicalResourceId lhs,
            LogicalResourceId rhs) noexcept
        {
            return lhs.domain < rhs.domain
                || (lhs.domain == rhs.domain && lhs.value < rhs.value);
        }

        [[nodiscard]] constexpr bool handle_less(
            SourceHandle lhs,
            SourceHandle rhs) noexcept
        {
            return lhs.index < rhs.index
                || (lhs.index == rhs.index
                    && lhs.generation < rhs.generation);
        }

        [[nodiscard]] bool finite(float value) noexcept
        {
            return std::isfinite(value);
        }

        [[nodiscard]] bool finite(double value) noexcept
        {
            return std::isfinite(value);
        }

        [[nodiscard]] bool finite(const Vector3& value) noexcept
        {
            return finite(value.x) && finite(value.y) && finite(value.z);
        }

        [[nodiscard]] bool finite(const SpatialState& value) noexcept
        {
            return finite(value.position)
                && finite(value.velocity)
                && finite(value.reference_distance)
                && finite(value.maximum_distance)
                && finite(value.rolloff)
                && finite(value.minimum_gain)
                && finite(value.maximum_gain);
        }

        [[nodiscard]] bool finite(const ListenerState& value) noexcept
        {
            return finite(value.position)
                && finite(value.velocity)
                && finite(value.forward)
                && finite(value.up)
                && finite(value.world_units_per_metre);
        }

        [[nodiscard]] double wrap_position(
            double value,
            double duration) noexcept
        {
            double wrapped = std::fmod(value, duration);
            if (wrapped < 0.0)
                wrapped += duration;
            return wrapped;
        }

        [[nodiscard]] bool nearly_equal(
            double lhs,
            double rhs,
            double scale) noexcept
        {
            const double tolerance = (std::max)(
                1.0e-10,
                (std::max)(1.0, scale)
                    * 64.0 * std::numeric_limits<double>::epsilon());
            return std::abs(lhs - rhs) <= tolerance;
        }

        struct StereoGain final
        {
            double left{ 1.0 };
            double right{ 1.0 };
        };

        [[nodiscard]] StereoGain spatial_gain(
            const ListenerState& listener,
            const SpatialState& source) noexcept
        {
            if (!source.enabled)
                return {};

            const double scale = static_cast<double>(
                listener.world_units_per_metre);
            const double dx = static_cast<double>(source.position.x)
                - static_cast<double>(listener.position.x);
            const double dy = static_cast<double>(source.position.y)
                - static_cast<double>(listener.position.y);
            const double dz = static_cast<double>(source.position.z)
                - static_cast<double>(listener.position.z);
            const double world_distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            const double distance = world_distance / scale;
            const double reference = static_cast<double>(source.reference_distance);
            const double maximum = static_cast<double>(source.maximum_distance);
            const double clamped_distance = (std::clamp)(
                distance,
                reference,
                maximum);
            const double denominator = reference
                + static_cast<double>(source.rolloff)
                    * (clamped_distance - reference);
            const double raw_attenuation = denominator > 0.0
                ? reference / denominator
                : 1.0;
            const double attenuation = (std::clamp)(
                raw_attenuation,
                static_cast<double>(source.minimum_gain),
                static_cast<double>(source.maximum_gain));

            double rx = static_cast<double>(listener.forward.y)
                    * static_cast<double>(listener.up.z)
                - static_cast<double>(listener.forward.z)
                    * static_cast<double>(listener.up.y);
            double ry = static_cast<double>(listener.forward.z)
                    * static_cast<double>(listener.up.x)
                - static_cast<double>(listener.forward.x)
                    * static_cast<double>(listener.up.z);
            double rz = static_cast<double>(listener.forward.x)
                    * static_cast<double>(listener.up.y)
                - static_cast<double>(listener.forward.y)
                    * static_cast<double>(listener.up.x);
            const double right_length = std::sqrt(rx * rx + ry * ry + rz * rz);
            if (right_length > 1.0e-12)
            {
                rx /= right_length;
                ry /= right_length;
                rz /= right_length;
            }
            else
            {
                rx = 1.0;
                ry = 0.0;
                rz = 0.0;
            }

            double pan{};
            if (world_distance > 1.0e-12)
            {
                pan = (std::clamp)(
                    (dx * rx + dy * ry + dz * rz) / world_distance,
                    -1.0,
                    1.0);
            }

            return {
                .left = attenuation * (pan > 0.0 ? 1.0 - pan : 1.0),
                .right = attenuation * (pan < 0.0 ? 1.0 + pan : 1.0)
            };
        }
    }

    struct AudioMixer::Implementation final
    {
        struct ClipEntry final
        {
            OwnedPcmClip clip{};
            PcmClipDescriptor descriptor{};
        };

        explicit Implementation(AudioMixerLimits requested_limits)
            : limits{ sanitize_limits(requested_limits) }
        {
            clips.reserve(limits.maximum_clips);
            buses.reserve(limits.maximum_bus_bindings);
            refresh_residency_metrics();
        }

        [[nodiscard]] auto find_clip(ClipId id) noexcept
        {
            return std::lower_bound(
                clips.begin(),
                clips.end(),
                id,
                [](const ClipEntry& entry, ClipId candidate)
                {
                    return logical_id_less(entry.clip.id, candidate);
                });
        }

        [[nodiscard]] auto find_clip(ClipId id) const noexcept
        {
            return std::lower_bound(
                clips.begin(),
                clips.end(),
                id,
                [](const ClipEntry& entry, ClipId candidate)
                {
                    return logical_id_less(entry.clip.id, candidate);
                });
        }

        [[nodiscard]] auto find_bus(LogicalResourceId id) noexcept
        {
            return std::lower_bound(
                buses.begin(),
                buses.end(),
                id,
                [](const LogicalBusBinding& binding, LogicalResourceId candidate)
                {
                    return logical_id_less(binding.logical_id, candidate);
                });
        }

        [[nodiscard]] auto find_bus(LogicalResourceId id) const noexcept
        {
            return std::lower_bound(
                buses.begin(),
                buses.end(),
                id,
                [](const LogicalBusBinding& binding, LogicalResourceId candidate)
                {
                    return logical_id_less(binding.logical_id, candidate);
                });
        }

        [[nodiscard]] auto find_runtime_bus(BusHandle handle) const noexcept
        {
            return std::find_if(
                buses.begin(),
                buses.end(),
                [handle](const LogicalBusBinding& binding)
                {
                    return binding.runtime_bus == handle;
                });
        }

        void refresh_residency_metrics() noexcept
        {
            cumulative.resident_clips = static_cast<std::uint32_t>(clips.size());
            cumulative.bound_buses = static_cast<std::uint32_t>(buses.size());
            cumulative.resident_frames = resident_frames;
            cumulative.resident_samples = resident_samples;
            cumulative.resident_bytes = resident_samples * sizeof(float);
            cumulative.peak_resident_samples = (std::max)(
                cumulative.peak_resident_samples,
                resident_samples);
        }

        [[nodiscard]] AudioMixerCode validate_plan(
            const FrameMixPlan& plan,
            const AudioMixRequest& request,
            std::vector<const ClipEntry*>& resolved_clips,
            std::vector<const LogicalBusBinding*>& resolved_buses) const
        {
            if (!supported_output_format(request.output_format))
                return AudioMixerCode::unsupported_output_format;
            if (request.output_frame_count == 0
                || request.output_frame_count
                    > limits.maximum_output_frames_per_mix)
            {
                return AudioMixerCode::invalid_output_frame_count;
            }
            if (!plan.logical_schedule_ready()
                || plan.physical_output_available()
                || plan.physical_output
                    != PhysicalOutputState::unavailable_no_backend
                || !finite(plan.timeline_seconds)
                || !finite(plan.duration_seconds)
                || !finite(plan.listener)
                || plan.timeline_seconds < 0.0
                || plan.duration_seconds <= 0.0
                || plan.sources.size() > limits.maximum_sources_per_mix)
            {
                return AudioMixerCode::invalid_frame_plan;
            }

            const double exact_output_frames = plan.duration_seconds
                * static_cast<double>(request.output_format.sample_rate);
            if (!finite(exact_output_frames)
                || exact_output_frames
                    > static_cast<double>(
                        (std::numeric_limits<std::uint64_t>::max)()))
            {
                return AudioMixerCode::frame_duration_mismatch;
            }
            const auto rounded_output_frames = static_cast<std::uint64_t>(
                std::llround(exact_output_frames));
            if (rounded_output_frames != request.output_frame_count
                || std::abs(
                    plan.duration_seconds
                        - static_cast<double>(request.output_frame_count)
                            / static_cast<double>(
                                request.output_format.sample_rate))
                    > 0.5
                        / static_cast<double>(request.output_format.sample_rate)
                        + 1.0e-12)
            {
                return AudioMixerCode::frame_duration_mismatch;
            }

            resolved_clips.reserve(plan.sources.size());
            resolved_buses.reserve(plan.sources.size());
            std::optional<SourceHandle> previous_source{};
            for (const auto& source : plan.sources)
            {
                if (!source.source.valid()
                    || !source.clip.valid()
                    || !source.bus.valid()
                    || (previous_source
                        && !handle_less(*previous_source, source.source))
                    || !finite(source.cursor_begin_seconds)
                    || !finite(source.cursor_end_seconds)
                    || !finite(source.source_gain)
                    || !finite(source.effective_bus_gain)
                    || !finite(source.effective_gain)
                    || !finite(source.pitch)
                    || !finite(source.spatial)
                    || source.cursor_begin_seconds < 0.0
                    || source.cursor_end_seconds < 0.0
                    || source.source_gain < 0.0f
                    || source.effective_bus_gain < 0.0f
                    || source.effective_gain < 0.0f
                    || source.pitch <= 0.0f
                    || source.logically_audible
                        != (source.effective_gain > 0.0f)
                    || (source.direction != PlaybackDirection::forward
                        && source.direction != PlaybackDirection::reverse))
                {
                    return AudioMixerCode::invalid_frame_plan;
                }
                previous_source = source.source;

                const auto clip = find_clip(source.clip);
                if (clip == clips.end() || clip->clip.id != source.clip)
                    return AudioMixerCode::missing_clip;
                const auto bus = find_runtime_bus(source.bus);
                if (bus == buses.end())
                    return AudioMixerCode::missing_bus;

                const double clip_duration =
                    static_cast<double>(clip->clip.frame_count)
                    / static_cast<double>(clip->clip.format.sample_rate);
                if (source.cursor_begin_seconds > clip_duration + 1.0e-10
                    || source.cursor_end_seconds > clip_duration + 1.0e-10)
                {
                    return AudioMixerCode::invalid_frame_plan;
                }

                const double direction = source.direction
                        == PlaybackDirection::forward
                    ? 1.0
                    : -1.0;
                const double raw_end = source.cursor_begin_seconds
                    + plan.duration_seconds
                        * static_cast<double>(source.pitch)
                        * direction;
                const double expected_end = source.looping
                    ? wrap_position(raw_end, clip_duration)
                    : (std::clamp)(raw_end, 0.0, clip_duration);
                if (!nearly_equal(
                        source.cursor_end_seconds,
                        expected_end,
                        clip_duration))
                {
                    return AudioMixerCode::invalid_frame_plan;
                }

                resolved_clips.push_back(std::addressof(*clip));
                resolved_buses.push_back(std::addressof(*bus));
            }
            return AudioMixerCode::success;
        }

        AudioMixerLimits limits{};
        std::vector<ClipEntry> clips{};
        std::vector<LogicalBusBinding> buses{};
        std::uint64_t resident_frames{};
        std::uint64_t resident_samples{};
        AudioMixerMetrics cumulative{};
        mutable std::mutex mutex{};
    };

    AudioMixer::AudioMixer(AudioMixerLimits limits)
        : implementation_{ std::make_unique<Implementation>(limits) }
    {
    }

    AudioMixer::~AudioMixer() = default;
    AudioMixer::AudioMixer(AudioMixer&&) noexcept = default;
    AudioMixer& AudioMixer::operator=(AudioMixer&&) noexcept = default;

    const AudioMixerLimits& AudioMixer::limits() const noexcept
    {
        return implementation_->limits;
    }

    AudioMixerCode AudioMixer::register_clip(OwnedPcmClip clip)
    {
        std::scoped_lock lock{ implementation_->mutex };
        if (!clip.id.valid() || clip.frame_count == 0)
            return AudioMixerCode::malformed_clip;
        if (!supported_clip_format(clip.format))
            return AudioMixerCode::unsupported_clip_format;
        if (clip.frame_count > implementation_->limits.maximum_frames_per_clip)
            return AudioMixerCode::capacity_exceeded;

        const std::uint64_t channels = channel_count(clip.format.channel_layout);
        if (!multiply_fits(clip.frame_count, channels))
            return AudioMixerCode::malformed_clip;
        const std::uint64_t expected_samples = clip.frame_count * channels;
        if (expected_samples != clip.interleaved_samples.size())
            return AudioMixerCode::malformed_clip;
        if (std::any_of(
                clip.interleaved_samples.begin(),
                clip.interleaved_samples.end(),
                [](float sample) { return !finite(sample); }))
        {
            return AudioMixerCode::malformed_clip;
        }

        auto position = implementation_->find_clip(clip.id);
        if (position != implementation_->clips.end()
            && position->clip.id == clip.id)
        {
            return AudioMixerCode::duplicate_clip;
        }
        if (implementation_->clips.size()
            >= implementation_->limits.maximum_clips)
        {
            return AudioMixerCode::capacity_exceeded;
        }
        if (expected_samples
            > implementation_->limits.maximum_resident_samples
                - implementation_->resident_samples)
        {
            return AudioMixerCode::sample_budget_exceeded;
        }

        const PcmClipDescriptor descriptor{
            .id = clip.id,
            .format = clip.format,
            .frame_count = clip.frame_count,
            .sample_count = expected_samples,
            .content_digest = clip_digest(clip)
        };
        implementation_->resident_frames += clip.frame_count;
        implementation_->resident_samples += expected_samples;
        implementation_->clips.insert(
            position,
            Implementation::ClipEntry{
                .clip = std::move(clip),
                .descriptor = descriptor
            });
        implementation_->refresh_residency_metrics();
        return AudioMixerCode::success;
    }

    AudioMixerCode AudioMixer::unregister_clip(ClipId clip)
    {
        std::scoped_lock lock{ implementation_->mutex };
        const auto position = implementation_->find_clip(clip);
        if (position == implementation_->clips.end()
            || position->clip.id != clip)
        {
            return AudioMixerCode::missing_clip;
        }
        implementation_->resident_frames -= position->descriptor.frame_count;
        implementation_->resident_samples -= position->descriptor.sample_count;
        implementation_->clips.erase(position);
        implementation_->refresh_residency_metrics();
        return AudioMixerCode::success;
    }

    bool AudioMixer::contains_clip(ClipId clip) const noexcept
    {
        std::scoped_lock lock{ implementation_->mutex };
        const auto position = implementation_->find_clip(clip);
        return position != implementation_->clips.end()
            && position->clip.id == clip;
    }

    std::optional<PcmClipDescriptor> AudioMixer::clip_descriptor(
        ClipId clip) const
    {
        std::scoped_lock lock{ implementation_->mutex };
        const auto position = implementation_->find_clip(clip);
        if (position == implementation_->clips.end()
            || position->clip.id != clip)
        {
            return std::nullopt;
        }
        return position->descriptor;
    }

    AudioMixerCode AudioMixer::bind_bus(const LogicalBusBinding& binding)
    {
        std::scoped_lock lock{ implementation_->mutex };
        if (!binding.logical_id.valid() || !binding.runtime_bus.valid())
            return AudioMixerCode::invalid_frame_plan;

        auto position = implementation_->find_bus(binding.logical_id);
        if (position != implementation_->buses.end()
            && position->logical_id == binding.logical_id)
        {
            return AudioMixerCode::duplicate_bus_identity;
        }
        if (implementation_->find_runtime_bus(binding.runtime_bus)
            != implementation_->buses.end())
        {
            return AudioMixerCode::duplicate_bus_handle;
        }
        if (implementation_->buses.size()
            >= implementation_->limits.maximum_bus_bindings)
        {
            return AudioMixerCode::capacity_exceeded;
        }
        implementation_->buses.insert(position, binding);
        implementation_->refresh_residency_metrics();
        return AudioMixerCode::success;
    }

    AudioMixerCode AudioMixer::unbind_bus(LogicalResourceId logical_bus)
    {
        std::scoped_lock lock{ implementation_->mutex };
        const auto position = implementation_->find_bus(logical_bus);
        if (position == implementation_->buses.end()
            || position->logical_id != logical_bus)
        {
            return AudioMixerCode::missing_bus;
        }
        implementation_->buses.erase(position);
        implementation_->refresh_residency_metrics();
        return AudioMixerCode::success;
    }

    std::optional<LogicalBusBinding> AudioMixer::bus_binding(
        BusHandle runtime_bus) const
    {
        std::scoped_lock lock{ implementation_->mutex };
        const auto position = implementation_->find_runtime_bus(runtime_bus);
        if (position == implementation_->buses.end())
            return std::nullopt;
        return *position;
    }

    MixedAudioFrame AudioMixer::mix(
        const FrameMixPlan& plan,
        const AudioMixRequest& request)
    {
        std::scoped_lock lock{ implementation_->mutex };
        saturating_add(
            implementation_->cumulative.mix_requests,
            std::uint64_t{ 1 });

        MixedAudioFrame result{
            .code = AudioMixerCode::invalid_frame_plan,
            .format = request.output_format,
            .plan_frame_index = plan.frame_index,
            .metrics = AudioMixFrameMetrics{
                .requested_frames = request.output_frame_count
            }
        };

        std::vector<const Implementation::ClipEntry*> resolved_clips;
        std::vector<const LogicalBusBinding*> resolved_buses;
        const AudioMixerCode validation = implementation_->validate_plan(
            plan,
            request,
            resolved_clips,
            resolved_buses);
        if (validation != AudioMixerCode::success)
        {
            result.code = validation;
            saturating_add(
                implementation_->cumulative.rejected_mix_requests,
                std::uint64_t{ 1 });
            return result;
        }

        result.metrics.planned_sources = static_cast<std::uint32_t>(
            plan.sources.size());
        result.logical_buses.reserve(resolved_buses.size());
        for (const auto* binding : resolved_buses)
        {
            const auto position = std::lower_bound(
                result.logical_buses.begin(),
                result.logical_buses.end(),
                binding->logical_id,
                logical_id_less);
            if (position == result.logical_buses.end()
                || *position != binding->logical_id)
            {
                result.logical_buses.insert(position, binding->logical_id);
            }
        }
        result.metrics.logical_buses_used = static_cast<std::uint32_t>(
            result.logical_buses.size());

        const std::size_t output_sample_count =
            static_cast<std::size_t>(request.output_frame_count) * 2u;
        std::vector<double> accumulation(output_sample_count, 0.0);

        for (std::size_t source_index = 0;
             source_index < plan.sources.size();
             ++source_index)
        {
            const auto& source = plan.sources[source_index];
            const auto& clip = resolved_clips[source_index]->clip;
            if (!source.logically_audible)
            {
                ++result.metrics.muted_sources;
                continue;
            }
            ++result.metrics.mixed_sources;

            const std::uint32_t input_channels = channel_count(
                clip.format.channel_layout);
            const double clip_duration = static_cast<double>(clip.frame_count)
                / static_cast<double>(clip.format.sample_rate);
            const double direction = source.direction
                    == PlaybackDirection::forward
                ? 1.0
                : -1.0;
            const StereoGain panning = spatial_gain(plan.listener, source.spatial);
            const double left_gain = static_cast<double>(source.effective_gain)
                * panning.left;
            const double right_gain = static_cast<double>(source.effective_gain)
                * panning.right;

            std::int64_t previous_loop_cycle{};
            bool loop_cycle_initialized{};
            for (std::uint32_t output_frame = 0;
                 output_frame < request.output_frame_count;
                 ++output_frame)
            {
                double position = source.cursor_begin_seconds
                    + direction * static_cast<double>(source.pitch)
                        * static_cast<double>(output_frame)
                        / static_cast<double>(request.output_format.sample_rate);
                if (source.looping)
                {
                    const auto loop_cycle = static_cast<std::int64_t>(
                        std::floor(position / clip_duration));
                    if (loop_cycle_initialized)
                    {
                        const auto crossings = loop_cycle >= previous_loop_cycle
                            ? static_cast<std::uint64_t>(
                                loop_cycle - previous_loop_cycle)
                            : static_cast<std::uint64_t>(
                                previous_loop_cycle - loop_cycle);
                        saturating_add(result.metrics.loop_wraps, crossings);
                    }
                    previous_loop_cycle = loop_cycle;
                    loop_cycle_initialized = true;
                    position = wrap_position(position, clip_duration);
                }
                else if ((source.direction == PlaybackDirection::forward
                            && position >= clip_duration)
                    || (source.direction == PlaybackDirection::reverse
                        && position < 0.0))
                {
                    continue;
                }

                if (source.direction == PlaybackDirection::reverse
                    && position >= clip_duration)
                {
                    position = std::nextafter(clip_duration, 0.0);
                }

                const double input_coordinate = position
                    * static_cast<double>(clip.format.sample_rate);
                std::uint64_t first_frame = static_cast<std::uint64_t>(
                    std::floor(input_coordinate));
                if (first_frame >= clip.frame_count)
                    first_frame = clip.frame_count - 1;
                const double fraction = (std::clamp)(
                    input_coordinate - static_cast<double>(first_frame),
                    0.0,
                    1.0);
                std::uint64_t second_frame = first_frame + 1;
                if (second_frame >= clip.frame_count)
                    second_frame = source.looping ? 0 : first_frame;

                const auto sample = [&](std::uint64_t frame, std::uint32_t channel)
                {
                    const std::size_t offset = static_cast<std::size_t>(
                        frame * input_channels + channel);
                    return static_cast<double>(clip.interleaved_samples[offset]);
                };
                const auto interpolate = [&](std::uint32_t channel)
                {
                    const double first = sample(first_frame, channel);
                    const double second = sample(second_frame, channel);
                    return first + (second - first) * fraction;
                };

                const double left = interpolate(0);
                const double right = input_channels == 1
                    ? left
                    : interpolate(1);
                const std::size_t output_offset =
                    static_cast<std::size_t>(output_frame) * 2u;
                accumulation[output_offset] += left * left_gain;
                accumulation[output_offset + 1] += right * right_gain;
                saturating_add(
                    result.metrics.source_frames_sampled,
                    std::uint64_t{ 1 });
                if (fraction > 0.0 && second_frame != first_frame)
                {
                    saturating_add(
                        result.metrics.interpolated_source_frames,
                        std::uint64_t{ 1 });
                }
            }
        }

        result.interleaved_samples.resize(output_sample_count);
        for (std::size_t index = 0; index < accumulation.size(); ++index)
        {
            const double mixed = accumulation[index];
            const double clipped = (std::clamp)(mixed, -1.0, 1.0);
            if (clipped != mixed)
                saturating_add(
                    result.metrics.clipped_output_samples,
                    std::uint64_t{ 1 });
            result.interleaved_samples[index] = static_cast<float>(clipped);
        }

        result.code = AudioMixerCode::success;
        result.metrics.produced_frames = request.output_frame_count;
        saturating_add(
            implementation_->cumulative.frames_produced,
            static_cast<std::uint64_t>(request.output_frame_count));
        saturating_add(
            implementation_->cumulative.source_frames_sampled,
            result.metrics.source_frames_sampled);
        saturating_add(
            implementation_->cumulative.clipped_output_samples,
            result.metrics.clipped_output_samples);
        return result;
    }

    AudioMixerSnapshot AudioMixer::snapshot() const
    {
        std::scoped_lock lock{ implementation_->mutex };
        AudioMixerSnapshot result{};
        result.clips.reserve(implementation_->clips.size());
        for (const auto& clip : implementation_->clips)
            result.clips.push_back(clip.descriptor);
        result.buses = implementation_->buses;
        result.metrics = implementation_->cumulative;
        return result;
    }

    AudioMixerCode AudioMixer::restore(const AudioMixerSnapshot& snapshot)
    {
        std::scoped_lock lock{ implementation_->mutex };
        if (snapshot.clips.size() > implementation_->limits.maximum_clips
            || snapshot.buses.size()
                > implementation_->limits.maximum_bus_bindings
            || snapshot.metrics.resident_clips != snapshot.clips.size()
            || snapshot.metrics.bound_buses != snapshot.buses.size()
            || snapshot.metrics.resident_samples
                > implementation_->limits.maximum_resident_samples
            || snapshot.metrics.resident_bytes
                != snapshot.metrics.resident_samples * sizeof(float)
            || snapshot.metrics.peak_resident_samples
                < snapshot.metrics.resident_samples)
        {
            return AudioMixerCode::invalid_snapshot;
        }

        if (snapshot.clips.size() != implementation_->clips.size()
            || snapshot.buses != implementation_->buses)
        {
            return AudioMixerCode::snapshot_registry_mismatch;
        }
        for (std::size_t index = 0; index < snapshot.clips.size(); ++index)
        {
            if (snapshot.clips[index]
                != implementation_->clips[index].descriptor)
            {
                return AudioMixerCode::snapshot_registry_mismatch;
            }
        }
        if (snapshot.metrics.resident_frames != implementation_->resident_frames
            || snapshot.metrics.resident_samples
                != implementation_->resident_samples)
        {
            return AudioMixerCode::snapshot_registry_mismatch;
        }

        implementation_->cumulative = snapshot.metrics;
        implementation_->refresh_residency_metrics();
        return AudioMixerCode::success;
    }

    void AudioMixer::reset() noexcept
    {
        std::scoped_lock lock{ implementation_->mutex };
        const std::uint64_t resets = implementation_->cumulative.resets;
        implementation_->cumulative = {};
        implementation_->cumulative.resets = resets;
        saturating_add(
            implementation_->cumulative.resets,
            std::uint64_t{ 1 });
        implementation_->refresh_residency_metrics();
    }

    void AudioMixer::teardown() noexcept
    {
        std::scoped_lock lock{ implementation_->mutex };
        implementation_->clips.clear();
        implementation_->buses.clear();
        implementation_->resident_frames = 0;
        implementation_->resident_samples = 0;
        implementation_->cumulative = {};
        implementation_->refresh_residency_metrics();
    }

    AudioMixerMetrics AudioMixer::metrics() const noexcept
    {
        std::scoped_lock lock{ implementation_->mutex };
        return implementation_->cumulative;
    }
}
