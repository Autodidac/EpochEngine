/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

module audio.playback_runtime;

namespace epochengine::audio
{
    namespace
    {
        constexpr std::uint32_t output_sample_rate = 48'000;
        constexpr std::uint16_t output_channels = 2;
        constexpr double maximum_frame_seconds = 0.25;

        [[nodiscard]] constexpr std::uint64_t next_generation(
            std::uint64_t value) noexcept
        {
            ++value;
            return value == 0 ? 1 : value;
        }

        [[nodiscard]] constexpr std::uint16_t channel_count(
            PcmChannelLayout layout) noexcept
        {
            switch (layout)
            {
            case PcmChannelLayout::mono: return 1;
            case PcmChannelLayout::stereo: return 2;
            case PcmChannelLayout::surround_5_1: return 6;
            }
            return 0;
        }

        [[nodiscard]] constexpr ClipResource clip_resource(
            const OwnedPcmClip& clip) noexcept
        {
            return {
                .id = clip.id,
                .duration_seconds = clip.format.sample_rate == 0
                    ? 0.0
                    : static_cast<double>(clip.frame_count)
                        / static_cast<double>(clip.format.sample_rate),
                .nominal_sample_rate = clip.format.sample_rate,
                .channel_count = channel_count(clip.format.channel_layout),
                .seekable = true,
                .reverse_decodable = true
            };
        }
    }

    struct PlaybackRuntime::Implementation final
    {
        struct Bus final
        {
            LogicalResourceId id{};
            BusHandle handle{};
        };

        struct Cue final
        {
            ClipId id{};
            LogicalResourceId bus{};
            SourceHandle source{};
        };

        explicit Implementation(
            std::unique_ptr<AudioDeviceSink> sink,
            AudioMixerLimits mixerLimits)
            : mixer{mixerLimits}
            , device{std::move(sink)}
        {
            const auto master = manager.bus_snapshot(manager.master_bus());
            if (!master
                || mixer.bind_bus({master->id, master->handle})
                    != AudioMixerCode::success)
            {
                diagnostic = "logical master bus could not be bound to the mixer";
            }
            else
            {
                diagnostic = "audio playback runtime is idle";
            }
        }

        [[nodiscard]] bool valid(PlaybackSessionHandle requested) const noexcept
        {
            return active && requested.valid() && requested == handle;
        }

        [[nodiscard]] Cue* find(ClipId id) noexcept
        {
            const auto found = std::find_if(cues.begin(), cues.end(),
                [id](const Cue& cue) noexcept { return cue.id == id; });
            return found == cues.end() ? nullptr : &*found;
        }

        [[nodiscard]] Bus* find_bus(LogicalResourceId id) noexcept
        {
            const auto found = std::find_if(buses.begin(), buses.end(),
                [id](const Bus& bus) noexcept { return bus.id == id; });
            return found == buses.end() ? nullptr : &*found;
        }

        [[nodiscard]] const Bus* find_bus(
            LogicalResourceId id) const noexcept
        {
            const auto found = std::find_if(buses.begin(), buses.end(),
                [id](const Bus& bus) noexcept { return bus.id == id; });
            return found == buses.end() ? nullptr : &*found;
        }

        [[nodiscard]] PlaybackRuntimeCode queue_for_all(
            bool pause,
            bool resetCursor)
        {
            const std::uint64_t target = frameIndex + 1u;
            std::uint32_t order{};
            for (const Cue& cue : cues)
            {
                AudioCommandPayload payload = resetCursor
                    ? AudioCommandPayload{StopCommand{
                        .source = cue.source,
                        .reset_cursor = true}}
                    : (pause
                        ? AudioCommandPayload{PauseCommand{cue.source}}
                        : AudioCommandPayload{ResumeCommand{cue.source}});
                const CommandReceipt receipt = manager.submit({
                    .target_frame = target,
                    .order_group = order++,
                    .payload = std::move(payload)
                });
                if (!receipt.accepted())
                    return PlaybackRuntimeCode::command_rejected;
            }
            return PlaybackRuntimeCode::success;
        }

        AudioManager manager{};
        AudioMixer mixer;
        PhysicalAudioDevice device;
        std::vector<Bus> buses{};
        std::vector<Cue> cues{};
        PlaybackSessionHandle handle{};
        std::uint64_t generation{};
        std::uint64_t stableSessionId{};
        std::uint64_t frameIndex{};
        double timelineSeconds{};
        bool active{};
        bool paused{};
        bool physicalRequested{};
        PlaybackRuntimeMetrics metrics{};
        std::string diagnostic{};
    };

    PlaybackRuntime::PlaybackRuntime(
        std::unique_ptr<AudioDeviceSink> physicalSink,
        AudioMixerLimits mixerLimits)
        : implementation_{std::make_unique<Implementation>(
            std::move(physicalSink), mixerLimits)}
    {
    }

    PlaybackRuntime::~PlaybackRuntime()
    {
        if (implementation_ && implementation_->active)
            (void)close_session(implementation_->handle);
        if (implementation_)
            implementation_->device.close();
    }

    PlaybackRuntime::PlaybackRuntime(PlaybackRuntime&&) noexcept = default;
    PlaybackRuntime& PlaybackRuntime::operator=(PlaybackRuntime&&) noexcept = default;

    PlaybackSessionResult PlaybackRuntime::open_session(
        PlaybackSessionRequest request)
    {
        auto& implementation = *implementation_;
        if (implementation.active)
            return {.code = PlaybackRuntimeCode::busy};
        if (request.stable_session_id == 0u || request.cues.empty()
            || request.cues.size()
                > implementation.mixer.limits().maximum_clips
            || request.buses.size() + 1u
                > implementation.manager.limits().maximum_buses
            || request.buses.size() + 1u
                > implementation.mixer.limits().maximum_bus_bindings)
        {
            return {.code = PlaybackRuntimeCode::invalid_request};
        }

        for (std::size_t index = 0; index < request.buses.size(); ++index)
        {
            const PlaybackBusDefinition& bus = request.buses[index];
            if (!bus.id.valid() || !std::isfinite(bus.gain)
                || bus.gain < 0.0f || bus.gain > 16.0f
                || bus.parent == bus.id)
            {
                return {.code = PlaybackRuntimeCode::invalid_request};
            }

            const auto duplicate = std::find_if(
                request.buses.begin(),
                request.buses.begin()
                    + static_cast<std::ptrdiff_t>(index),
                [&bus](const PlaybackBusDefinition& previous) noexcept
                {
                    return previous.id == bus.id;
                });
            if (duplicate != request.buses.begin()
                    + static_cast<std::ptrdiff_t>(index))
            {
                return {.code = PlaybackRuntimeCode::bus_rejected};
            }

            if (bus.parent.valid())
            {
                const auto parent = std::find_if(
                    request.buses.begin(),
                    request.buses.begin()
                        + static_cast<std::ptrdiff_t>(index),
                    [&bus](const PlaybackBusDefinition& previous) noexcept
                    {
                        return previous.id == bus.parent;
                    });
                if (parent == request.buses.begin()
                        + static_cast<std::ptrdiff_t>(index))
                {
                    return {.code = PlaybackRuntimeCode::bus_rejected};
                }
            }
        }

        for (std::size_t index = 0; index < request.cues.size(); ++index)
        {
            const PlaybackCueDefinition& cue = request.cues[index];
            if (!cue.clip.id.valid() || !std::isfinite(cue.gain)
                || cue.gain < 0.0f || cue.gain > 16.0f)
            {
                return {.code = PlaybackRuntimeCode::invalid_request};
            }
            for (std::size_t previous = 0; previous < index; ++previous)
            {
                if (request.cues[previous].clip.id == cue.clip.id)
                    return {.code = PlaybackRuntimeCode::duplicate_cue};
            }
            if (cue.bus.valid())
            {
                const auto route = std::find_if(
                    request.buses.begin(),
                    request.buses.end(),
                    [&cue](const PlaybackBusDefinition& bus) noexcept
                    {
                        return bus.id == cue.bus;
                    });
                if (route == request.buses.end())
                    return {.code = PlaybackRuntimeCode::bus_rejected};
            }
        }

        std::vector<Implementation::Bus> buses{};
        std::vector<ClipId> registered{};
        std::vector<SourceHandle> sources{};
        buses.reserve(request.buses.size());
        registered.reserve(request.cues.size());
        sources.reserve(request.cues.size());
        const auto rollback = [&]() noexcept
        {
            for (SourceHandle source : sources)
                (void)implementation.manager.destroy_source(source);
            for (ClipId clip : registered)
                (void)implementation.mixer.unregister_clip(clip);
            for (auto bus = buses.rbegin(); bus != buses.rend(); ++bus)
            {
                (void)implementation.mixer.unbind_bus(bus->id);
                (void)implementation.manager.destroy_bus(bus->handle);
            }
        };

        for (const PlaybackBusDefinition& definition : request.buses)
        {
            BusHandle parent = implementation.manager.master_bus();
            if (definition.parent.valid())
            {
                const auto found = std::find_if(
                    buses.begin(),
                    buses.end(),
                    [&definition](const Implementation::Bus& bus) noexcept
                    {
                        return bus.id == definition.parent;
                    });
                if (found == buses.end())
                {
                    rollback();
                    return {.code = PlaybackRuntimeCode::bus_rejected};
                }
                parent = found->handle;
            }

            const auto created = implementation.manager.create_bus({
                .id = definition.id,
                .parent = parent,
                .gain = definition.gain,
                .muted = definition.muted});
            if (!created.succeeded()
                || implementation.mixer.bind_bus({
                    definition.id, created.handle})
                    != AudioMixerCode::success)
            {
                if (created.succeeded())
                    (void)implementation.manager.destroy_bus(created.handle);
                rollback();
                return {.code = PlaybackRuntimeCode::bus_rejected};
            }
            buses.push_back({definition.id, created.handle});
        }

        const auto master =
            implementation.manager.bus_snapshot(
                implementation.manager.master_bus());
        if (!master)
        {
            rollback();
            return {.code = PlaybackRuntimeCode::bus_rejected};
        }

        std::vector<LogicalResourceId> cueBuses{};
        cueBuses.reserve(request.cues.size());
        for (PlaybackCueDefinition& cue : request.cues)
        {
            BusHandle route = implementation.manager.master_bus();
            LogicalResourceId routeId = master->id;
            if (cue.bus.valid())
            {
                const auto found = std::find_if(
                    buses.begin(),
                    buses.end(),
                    [&cue](const Implementation::Bus& bus) noexcept
                    {
                        return bus.id == cue.bus;
                    });
                if (found == buses.end())
                {
                    rollback();
                    return {.code = PlaybackRuntimeCode::bus_rejected};
                }
                route = found->handle;
                routeId = found->id;
            }

            const ClipResource resource = clip_resource(cue.clip);
            const ClipId id = cue.clip.id;
            if (implementation.mixer.register_clip(std::move(cue.clip))
                != AudioMixerCode::success)
            {
                rollback();
                return {.code = PlaybackRuntimeCode::clip_rejected};
            }
            registered.push_back(id);
            const auto source = implementation.manager.create_source({
                .clip = resource,
                .bus = route,
                .gain = cue.gain,
                .pitch = 1.0f,
                .looping = cue.looping});
            if (!source.succeeded())
            {
                rollback();
                return {.code = PlaybackRuntimeCode::source_rejected};
            }
            sources.push_back(source.handle);
            cueBuses.push_back(routeId);
        }

        implementation.buses = std::move(buses);
        implementation.cues.clear();
        implementation.cues.reserve(registered.size());
        for (std::size_t index = 0; index < registered.size(); ++index)
        {
            implementation.cues.push_back({
                registered[index], cueBuses[index], sources[index]});
        }
        implementation.generation = next_generation(implementation.generation);
        implementation.handle = {implementation.generation};
        implementation.stableSessionId = request.stable_session_id;
        implementation.active = true;
        implementation.paused = false;
        implementation.physicalRequested = request.request_physical_output;
        ++implementation.metrics.sessions_opened;
        implementation.metrics.cues_registered += implementation.cues.size();
        implementation.metrics.buses_registered +=
            implementation.buses.size();

        if (request.request_physical_output
            && implementation.device.state() != AudioDeviceState::ready)
        {
            const AudioDeviceCode opened = implementation.device.open({
                .sample_rate = output_sample_rate,
                .channel_count = output_channels,
                .maximum_submission_frames = 12'000,
                .maximum_queued_frames = 24'000});
            if (opened != AudioDeviceCode::success
                && opened != AudioDeviceCode::already_open
                && opened != AudioDeviceCode::unavailable)
            {
                ++implementation.metrics.physical_failures;
            }
        }
        if (implementation.device.state() == AudioDeviceState::paused)
            (void)implementation.device.set_paused(false);
        implementation.diagnostic = "audio playback session is ready";
        return {
            .code = PlaybackRuntimeCode::success,
            .handle = implementation.handle};
    }
    PlaybackRuntimeCode PlaybackRuntime::trigger(
        PlaybackSessionHandle session,
        ClipId cue,
        double startSeconds)
    {
        auto& implementation = *implementation_;
        if (!implementation.valid(session))
            return PlaybackRuntimeCode::invalid_session;
        Implementation::Cue* found = implementation.find(cue);
        if (!found || !std::isfinite(startSeconds) || startSeconds < 0.0)
        {
            ++implementation.metrics.triggers_rejected;
            return PlaybackRuntimeCode::invalid_request;
        }
        const CommandReceipt receipt = implementation.manager.submit({
            .target_frame = implementation.frameIndex + 1u,
            .order_group = 0,
            .payload = PlayCommand{
                .source = found->source,
                .start_seconds = startSeconds
            }
        });
        if (!receipt.accepted())
        {
            ++implementation.metrics.triggers_rejected;
            return PlaybackRuntimeCode::command_rejected;
        }
        ++implementation.metrics.triggers_accepted;
        return PlaybackRuntimeCode::success;
    }

    PlaybackRuntimeCode PlaybackRuntime::set_bus_gain(
        PlaybackSessionHandle session,
        LogicalResourceId bus,
        float gain)
    {
        auto& implementation = *implementation_;
        if (!implementation.valid(session))
            return PlaybackRuntimeCode::invalid_session;
        const Implementation::Bus* found = implementation.find_bus(bus);
        if (!found || !std::isfinite(gain) || gain < 0.0f || gain > 16.0f)
            return PlaybackRuntimeCode::invalid_request;
        const CommandReceipt receipt = implementation.manager.submit({
            .target_frame = implementation.frameIndex + 1u,
            .order_group = 0u,
            .payload = SetBusGainCommand{
                .bus = found->handle,
                .gain = gain}});
        if (!receipt.accepted())
            return PlaybackRuntimeCode::command_rejected;
        ++implementation.metrics.bus_control_transitions;
        return PlaybackRuntimeCode::success;
    }

    PlaybackRuntimeCode PlaybackRuntime::set_bus_muted(
        PlaybackSessionHandle session,
        LogicalResourceId bus,
        bool muted)
    {
        auto& implementation = *implementation_;
        if (!implementation.valid(session))
            return PlaybackRuntimeCode::invalid_session;
        const Implementation::Bus* found = implementation.find_bus(bus);
        if (!found)
            return PlaybackRuntimeCode::invalid_request;
        const CommandReceipt receipt = implementation.manager.submit({
            .target_frame = implementation.frameIndex + 1u,
            .order_group = 0u,
            .payload = SetBusMuteCommand{
                .bus = found->handle,
                .muted = muted}});
        if (!receipt.accepted())
            return PlaybackRuntimeCode::command_rejected;
        ++implementation.metrics.bus_control_transitions;
        return PlaybackRuntimeCode::success;
    }

    PlaybackRuntimeCode PlaybackRuntime::set_paused(
        PlaybackSessionHandle session,
        bool paused)
    {
        auto& implementation = *implementation_;
        if (!implementation.valid(session))
            return PlaybackRuntimeCode::invalid_session;
        if (implementation.paused == paused)
            return PlaybackRuntimeCode::success;
        const PlaybackRuntimeCode queued = implementation.queue_for_all(paused, false);
        if (queued != PlaybackRuntimeCode::success)
            return queued;
        if (implementation.physicalRequested
            && (implementation.device.state() == AudioDeviceState::ready
                || implementation.device.state() == AudioDeviceState::paused))
        {
            const AudioDeviceCode code = implementation.device.set_paused(paused);
            if (code != AudioDeviceCode::success)
            {
                ++implementation.metrics.physical_failures;
                implementation.diagnostic = "physical audio pause transition failed";
            }
        }
        implementation.paused = paused;
        ++implementation.metrics.pause_transitions;
        return PlaybackRuntimeCode::success;
    }

    PlaybackRuntimeCode PlaybackRuntime::reset(PlaybackSessionHandle session)
    {
        auto& implementation = *implementation_;
        if (!implementation.valid(session))
            return PlaybackRuntimeCode::invalid_session;
        const PlaybackRuntimeCode queued = implementation.queue_for_all(false, true);
        if (queued != PlaybackRuntimeCode::success)
            return queued;
        if (implementation.device.state() == AudioDeviceState::ready
            || implementation.device.state() == AudioDeviceState::paused)
        {
            (void)implementation.device.clear();
        }
        ++implementation.metrics.resets;
        return PlaybackRuntimeCode::success;
    }

    PlaybackAdvanceResult PlaybackRuntime::advance(
        PlaybackSessionHandle session,
        double durationSeconds)
    {
        auto& implementation = *implementation_;
        PlaybackAdvanceResult result{};
        result.physical_state = implementation.device.state();
        if (!implementation.valid(session))
            return result;
        if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0
            || durationSeconds > maximum_frame_seconds)
        {
            result.code = PlaybackRuntimeCode::invalid_request;
            return result;
        }

        ++implementation.frameIndex;
        const FrameMixPlan plan = implementation.manager.prepare_frame({
            .frame_index = implementation.frameIndex,
            .timeline_seconds = implementation.timelineSeconds,
            .duration_seconds = durationSeconds
        });
        if (!plan.logical_schedule_ready())
        {
            result.code = PlaybackRuntimeCode::frame_rejected;
            return result;
        }

        const auto roundedFrames = std::llround(
            durationSeconds * static_cast<double>(output_sample_rate));
        const std::uint32_t outputFrames = static_cast<std::uint32_t>(
            (std::clamp)(roundedFrames, 1ll, 12'000ll));
        result.mixed = implementation.mixer.mix(plan, {
            .output_format = {
                .sample_format = PcmSampleFormat::float32_interleaved,
                .channel_layout = PcmChannelLayout::stereo,
                .sample_rate = output_sample_rate
            },
            .output_frame_count = outputFrames
        });
        if (!result.mixed)
        {
            result.code = PlaybackRuntimeCode::mix_rejected;
            return result;
        }

        implementation.timelineSeconds += durationSeconds;
        ++implementation.metrics.frames_mixed;
        result.code = PlaybackRuntimeCode::success;
        result.frame_index = implementation.frameIndex;
        result.timeline_seconds = implementation.timelineSeconds;
        result.physical_code = AudioDeviceCode::unavailable;

        if (implementation.physicalRequested
            && !implementation.paused
            && implementation.device.state() == AudioDeviceState::ready)
        {
            result.physical_code = implementation.device.submit(
                result.mixed.interleaved_samples);
            result.physical_state = implementation.device.state();
            if (result.physical_code == AudioDeviceCode::success)
                ++implementation.metrics.frames_submitted;
            else if (result.physical_code == AudioDeviceCode::queue_saturated)
                result.code = PlaybackRuntimeCode::physical_queue_saturated;
            else
            {
                result.code = PlaybackRuntimeCode::physical_output_failed;
                ++implementation.metrics.physical_failures;
                implementation.diagnostic = "physical audio submission failed";
            }
        }
        return result;
    }

    PlaybackRuntimeCode PlaybackRuntime::close_session(
        PlaybackSessionHandle session) noexcept
    {
        auto& implementation = *implementation_;
        if (!implementation.valid(session))
            return PlaybackRuntimeCode::invalid_session;
        for (const Implementation::Cue& cue : implementation.cues)
        {
            (void)implementation.manager.destroy_source(cue.source);
            (void)implementation.mixer.unregister_clip(cue.id);
        }
        implementation.cues.clear();
        for (auto bus = implementation.buses.rbegin();
            bus != implementation.buses.rend();
            ++bus)
        {
            (void)implementation.mixer.unbind_bus(bus->id);
            (void)implementation.manager.destroy_bus(bus->handle);
        }
        implementation.buses.clear();
        if (implementation.device.state() == AudioDeviceState::ready
            || implementation.device.state() == AudioDeviceState::paused)
        {
            (void)implementation.device.clear();
            if (implementation.device.state() == AudioDeviceState::paused)
                (void)implementation.device.set_paused(false);
        }
        implementation.active = false;
        implementation.paused = false;
        implementation.physicalRequested = false;
        implementation.stableSessionId = 0;
        implementation.handle = {};
        ++implementation.metrics.sessions_closed;
        implementation.diagnostic = "audio playback runtime is idle";
        return PlaybackRuntimeCode::success;
    }

    PlaybackRuntimeSnapshot PlaybackRuntime::snapshot() const
    {
        const auto& implementation = *implementation_;
        PlaybackRuntimeSnapshot result{
            .session_active = implementation.active,
            .paused = implementation.paused,
            .stable_session_id = implementation.stableSessionId,
            .handle = implementation.handle,
            .frame_index = implementation.frameIndex,
            .timeline_seconds = implementation.timelineSeconds,
            .metrics = implementation.metrics,
            .mixer = implementation.mixer.metrics(),
            .device = implementation.device.snapshot(),
            .diagnostic = implementation.diagnostic
        };
        result.cues.reserve(implementation.cues.size());
        for (const Implementation::Cue& cue : implementation.cues)
            result.cues.push_back(cue.id);
        result.buses.reserve(implementation.buses.size());
        for (const Implementation::Bus& bus : implementation.buses)
            result.buses.push_back(bus.id);
        return result;
    }

    PlaybackRuntimeMetrics PlaybackRuntime::metrics() const noexcept
    {
        return implementation_->metrics;
    }
}
