/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

module audio.playback_runtime;

namespace epochengine::audio
{
    namespace
    {
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
            result.cues.push_back({
                .clip = cue_clip(cue),
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
        const PlaybackSessionResult opened = runtime.open_session(request(101u, cue));
        if (!opened || !runtime.snapshot().session_active)
            return 2;
        if (runtime.open_session(request(102u, {71u, 12u})).code
            != PlaybackRuntimeCode::busy)
        {
            return 3;
        }
        if (runtime.trigger({opened.handle.generation + 1u}, cue)
                != PlaybackRuntimeCode::invalid_session
            || runtime.trigger(opened.handle, {99u, 99u})
                != PlaybackRuntimeCode::invalid_request
            || runtime.trigger(opened.handle, cue) != PlaybackRuntimeCode::success)
        {
            return 4;
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
            return 5;
        }
        const std::vector<float> deterministic = first.mixed.interleaved_samples;

        if (runtime.set_paused(opened.handle, true)
                != PlaybackRuntimeCode::success
            || !runtime.snapshot().paused)
        {
            return 6;
        }
        const PlaybackAdvanceResult paused = runtime.advance(
            opened.handle, 1.0 / 60.0);
        if (!paused || !std::all_of(
                paused.mixed.interleaved_samples.begin(),
                paused.mixed.interleaved_samples.end(),
                [](float sample) noexcept { return sample == 0.0f; }))
        {
            return 7;
        }
        if (runtime.set_paused(opened.handle, false)
                != PlaybackRuntimeCode::success
            || runtime.reset(opened.handle) != PlaybackRuntimeCode::success)
        {
            return 8;
        }
        const PlaybackAdvanceResult reset = runtime.advance(
            opened.handle, 1.0 / 60.0);
        if (!reset || !std::all_of(
                reset.mixed.interleaved_samples.begin(),
                reset.mixed.interleaved_samples.end(),
                [](float sample) noexcept { return sample == 0.0f; }))
        {
            return 9;
        }

        if (runtime.close_session(opened.handle) != PlaybackRuntimeCode::success
            || runtime.close_session(opened.handle)
                != PlaybackRuntimeCode::invalid_session)
        {
            return 10;
        }
        const PlaybackSessionResult replay = runtime.open_session(request(101u, cue));
        if (!replay || replay.handle == opened.handle
            || runtime.trigger(replay.handle, cue) != PlaybackRuntimeCode::success)
        {
            return 11;
        }
        const PlaybackAdvanceResult replayed = runtime.advance(
            replay.handle, 1.0 / 60.0);
        if (!replayed || replayed.mixed.interleaved_samples != deterministic)
            return 12;

        const PlaybackRuntimeSnapshot snapshot = runtime.snapshot();
        if (snapshot.metrics.sessions_opened != 2u
            || snapshot.metrics.sessions_closed != 1u
            || snapshot.metrics.triggers_accepted != 2u
            || snapshot.metrics.triggers_rejected != 1u
            || snapshot.metrics.frames_mixed != 4u
            || snapshot.metrics.frames_submitted != 0u
            || snapshot.metrics.resets != 1u
            || snapshot.metrics.pause_transitions != 2u
            || snapshot.device.state != AudioDeviceState::unavailable)
        {
            return 13;
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
