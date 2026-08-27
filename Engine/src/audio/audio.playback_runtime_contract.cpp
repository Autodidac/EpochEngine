/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

module audio.playback_runtime;

namespace epochengine::audio
{
    namespace
    {
        inline constexpr LogicalResourceId effects_bus{71u, 101u};

        [[nodiscard]] OwnedPcmClip cue_clip(ClipId id)
        {
            constexpr std::uint32_t rate = 48'000;
            constexpr std::uint64_t frames = 4'800;
            OwnedPcmClip clip{
                .id = id,
                .format = {
                    .sample_format = PcmSampleFormat::float32_interleaved,
                    .channel_layout = PcmChannelLayout::mono,
                    .sample_rate = rate
                },
                .frame_count = frames
            };
            clip.interleaved_samples.resize(frames);
            for (std::uint64_t frame = 0; frame < frames; ++frame)
            {
                const float phase = static_cast<float>(frame % 48u) / 48.0f;
                clip.interleaved_samples[frame] = phase < 0.5f ? 0.25f : -0.25f;
            }
            return clip;
        }

        [[nodiscard]] PlaybackSessionRequest request(
            std::uint64_t session,
            ClipId cue)
        {
            PlaybackSessionRequest result{
                .stable_session_id = session,
                .request_physical_output = false
            };
            result.buses.push_back({
                .id = effects_bus,
                .gain = 1.0f,
                .muted = false});
            result.cues.push_back({
                .clip = cue_clip(cue),
                .bus = effects_bus,
                .gain = 0.5f,
                .looping = false
            });
            return result;
        }
    }

    int run_audio_playback_runtime_contract_tests()
    {
        const ClipId cue{71u, 11u};
        PlaybackRuntime runtime{};
        if (runtime.open_session({}).code != PlaybackRuntimeCode::invalid_request)
            return 1;
        PlaybackSessionRequest invalidBus = request(100u, cue);
        invalidBus.buses.front().parent = {99u, 99u};
        if (runtime.open_session(std::move(invalidBus)).code
            != PlaybackRuntimeCode::bus_rejected)
        {
            return 2;
        }
        const PlaybackSessionResult opened =
            runtime.open_session(request(101u, cue));
        const PlaybackRuntimeSnapshot openedSnapshot = runtime.snapshot();
        if (!opened || !openedSnapshot.session_active
            || openedSnapshot.buses.size() != 1u
            || openedSnapshot.buses.front() != effects_bus)
        {
            return 3;
        }
        if (runtime.open_session(request(102u, {71u, 12u})).code
            != PlaybackRuntimeCode::busy)
        {
            return 4;
        }
        if (runtime.trigger({opened.handle.generation + 1u}, cue)
                != PlaybackRuntimeCode::invalid_session
            || runtime.trigger(opened.handle, {99u, 99u})
                != PlaybackRuntimeCode::invalid_request
            || runtime.trigger(opened.handle, cue) != PlaybackRuntimeCode::success)
        {
            return 5;
        }

        const PlaybackAdvanceResult first = runtime.advance(
            opened.handle, 1.0 / 60.0);
        if (!first || !first.mixed || first.mixed.frame_count() != 800u
            || first.mixed.interleaved_samples.size() != 1'600u
            || std::all_of(
                first.mixed.interleaved_samples.begin(),
                first.mixed.interleaved_samples.end(),
                [](float sample) noexcept { return sample == 0.0f; }))
        {
            return 6;
        }
        const std::vector<float> deterministic = first.mixed.interleaved_samples;

        if (runtime.set_bus_gain(opened.handle, effects_bus, 0.75f)
                != PlaybackRuntimeCode::success
            || runtime.set_bus_muted(opened.handle, effects_bus, true)
                != PlaybackRuntimeCode::success
            || runtime.set_bus_gain(opened.handle, {99u, 99u}, 1.0f)
                != PlaybackRuntimeCode::invalid_request)
        {
            return 7;
        }
        const PlaybackAdvanceResult muted = runtime.advance(
            opened.handle, 1.0 / 60.0);
        if (!muted || !std::all_of(
                muted.mixed.interleaved_samples.begin(),
                muted.mixed.interleaved_samples.end(),
                [](float sample) noexcept { return sample == 0.0f; }))
        {
            return 8;
        }
        if (runtime.set_bus_muted(opened.handle, effects_bus, false)
                != PlaybackRuntimeCode::success)
        {
            return 9;
        }
        const PlaybackAdvanceResult unmuted = runtime.advance(
            opened.handle, 1.0 / 60.0);
        if (!unmuted || std::all_of(
                unmuted.mixed.interleaved_samples.begin(),
                unmuted.mixed.interleaved_samples.end(),
                [](float sample) noexcept { return sample == 0.0f; }))
        {
            return 10;
        }

        if (runtime.set_paused(opened.handle, true)
                != PlaybackRuntimeCode::success
            || !runtime.snapshot().paused)
        {
            return 11;
        }
        const PlaybackAdvanceResult paused = runtime.advance(
            opened.handle, 1.0 / 60.0);
        if (!paused || !std::all_of(
                paused.mixed.interleaved_samples.begin(),
                paused.mixed.interleaved_samples.end(),
                [](float sample) noexcept { return sample == 0.0f; }))
        {
            return 12;
        }
        if (runtime.set_paused(opened.handle, false)
                != PlaybackRuntimeCode::success
            || runtime.reset(opened.handle) != PlaybackRuntimeCode::success)
        {
            return 13;
        }
        const PlaybackAdvanceResult reset = runtime.advance(
            opened.handle, 1.0 / 60.0);
        if (!reset || !std::all_of(
                reset.mixed.interleaved_samples.begin(),
                reset.mixed.interleaved_samples.end(),
                [](float sample) noexcept { return sample == 0.0f; }))
        {
            return 14;
        }

        if (runtime.close_session(opened.handle)
                != PlaybackRuntimeCode::success
            || runtime.close_session(opened.handle)
                != PlaybackRuntimeCode::invalid_session)
        {
            return 15;
        }
        const PlaybackSessionResult replay =
            runtime.open_session(request(101u, cue));
        if (!replay || replay.handle == opened.handle
            || runtime.trigger(replay.handle, cue)
                != PlaybackRuntimeCode::success)
        {
            return 16;
        }
        const PlaybackAdvanceResult replayed = runtime.advance(
            replay.handle, 1.0 / 60.0);
        if (!replayed
            || replayed.mixed.interleaved_samples != deterministic)
        {
            return 17;
        }

        const PlaybackRuntimeSnapshot snapshot = runtime.snapshot();
        if (snapshot.metrics.sessions_opened != 2u
            || snapshot.metrics.sessions_closed != 1u
            || snapshot.metrics.cues_registered != 2u
            || snapshot.metrics.buses_registered != 2u
            || snapshot.metrics.bus_control_transitions != 3u
            || snapshot.metrics.triggers_accepted != 2u
            || snapshot.metrics.triggers_rejected != 1u
            || snapshot.metrics.frames_mixed != 6u
            || snapshot.metrics.frames_submitted != 0u
            || snapshot.metrics.resets != 1u
            || snapshot.metrics.pause_transitions != 2u
            || snapshot.buses.size() != 1u
            || snapshot.device.state != AudioDeviceState::unavailable)
        {
            return 18;
        }
        return 0;
    }
}

#if defined(EPOCH_AUDIO_PLAYBACK_RUNTIME_CONTRACT_MAIN)
int main()
{
    return epochengine::audio::run_audio_playback_runtime_contract_tests();
}
#endif
