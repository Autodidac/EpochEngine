/*
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

module audio.mixer;

namespace epochengine::audio
{
    namespace
    {
        constexpr std::uint32_t contract_sample_rate = 8'000;

        [[nodiscard]] OwnedPcmClip mono_clip(
            ClipId id,
            std::vector<float> samples)
        {
            const auto frame_count = static_cast<std::uint64_t>(samples.size());
            return {
                .id = id,
                .format = PcmFormat{
                    .sample_format = PcmSampleFormat::float32_interleaved,
                    .channel_layout = PcmChannelLayout::mono,
                    .sample_rate = contract_sample_rate
                },
                .frame_count = frame_count,
                .interleaved_samples = std::move(samples)
            };
        }

        [[nodiscard]] SourceDescriptor source_descriptor(
            AudioManager& manager,
            ClipId clip,
            std::uint64_t frame_count,
            float gain = 1.0f,
            bool looping = false)
        {
            return {
                .clip = ClipResource{
                    .id = clip,
                    .duration_seconds = static_cast<double>(frame_count)
                        / static_cast<double>(contract_sample_rate),
                    .nominal_sample_rate = contract_sample_rate,
                    .channel_count = 1,
                    .seekable = true,
                    .reverse_decodable = false
                },
                .bus = manager.master_bus(),
                .gain = gain,
                .pitch = 1.0f,
                .looping = looping
            };
        }

        [[nodiscard]] LogicalBusBinding master_binding(AudioManager& manager)
        {
            const auto master = manager.bus_snapshot(manager.master_bus());
            if (!master)
                return {};
            return {
                .logical_id = master->id,
                .runtime_bus = manager.master_bus()
            };
        }

        [[nodiscard]] AudioMixRequest mix_request(std::uint32_t frames) noexcept
        {
            return {
                .output_format = AudioOutputFormat{
                    .sample_format = PcmSampleFormat::float32_interleaved,
                    .channel_layout = PcmChannelLayout::stereo,
                    .sample_rate = contract_sample_rate
                },
                .output_frame_count = frames
            };
        }

        [[nodiscard]] FrameMixPlan prepare(
            AudioManager& manager,
            std::uint64_t frame,
            std::uint32_t output_frames)
        {
            return manager.prepare_frame(FrameRequest{
                .frame_index = frame,
                .timeline_seconds = static_cast<double>(frame)
                    * static_cast<double>(output_frames)
                    / static_cast<double>(contract_sample_rate),
                .duration_seconds = static_cast<double>(output_frames)
                    / static_cast<double>(contract_sample_rate)
            });
        }

        [[nodiscard]] bool exact_samples(
            const MixedAudioFrame& frame,
            const std::vector<float>& expected) noexcept
        {
            return frame.code == AudioMixerCode::success
                && frame.interleaved_samples == expected
                && frame.metrics.produced_frames * 2u == expected.size();
        }
    }

    AudioMixerContractReport run_audio_mixer_contract_tests()
    {
        AudioMixerContractReport report{};
        const auto pass = [&report]() { ++report.checks_completed; };
        const auto fail = [&report](AudioMixerContractFailure failure)
        {
            report.passed = false;
            report.failure = failure;
            return report;
        };

        const ClipId primary_id{ .domain = 41, .value = 1 };
        const ClipId second_id{ .domain = 41, .value = 2 };

        AudioMixer validation_mixer{ AudioMixerLimits{
            .maximum_clips = 2,
            .maximum_bus_bindings = 2,
            .maximum_sources_per_mix = 4,
            .maximum_frames_per_clip = 8,
            .maximum_resident_samples = 12,
            .maximum_output_frames_per_mix = 16
        } };
        OwnedPcmClip malformed = mono_clip(primary_id, { 0.0f, 1.0f });
        malformed.frame_count = 3;
        if (validation_mixer.register_clip(std::move(malformed))
                != AudioMixerCode::malformed_clip
            || validation_mixer.register_clip(OwnedPcmClip{
                .id = primary_id,
                .format = PcmFormat{
                    .sample_format = PcmSampleFormat::signed16_interleaved,
                    .channel_layout = PcmChannelLayout::mono,
                    .sample_rate = contract_sample_rate
                },
                .frame_count = 1,
                .interleaved_samples = { 0.0f }
            }) != AudioMixerCode::unsupported_clip_format
            || validation_mixer.register_clip(OwnedPcmClip{
                .id = primary_id,
                .format = PcmFormat{
                    .sample_format = PcmSampleFormat::float32_interleaved,
                    .channel_layout = PcmChannelLayout::mono,
                    .sample_rate = contract_sample_rate
                },
                .frame_count = 1,
                .interleaved_samples = {
                    (std::numeric_limits<float>::quiet_NaN)()
                }
            }) != AudioMixerCode::malformed_clip)
        {
            return fail(AudioMixerContractFailure::clip_validation_failed);
        }
        pass();

        if (validation_mixer.register_clip(
                mono_clip(primary_id, { 0.0f, 0.5f, 1.0f, 0.5f }))
                != AudioMixerCode::success
            || validation_mixer.register_clip(
                mono_clip(primary_id, { 0.0f }))
                != AudioMixerCode::duplicate_clip
            || !validation_mixer.contains_clip(primary_id)
            || !validation_mixer.clip_descriptor(primary_id))
        {
            return fail(AudioMixerContractFailure::clip_validation_failed);
        }
        pass();

        AudioManager manager{};
        const LogicalBusBinding master = master_binding(manager);
        if (!master.logical_id.valid()
            || validation_mixer.bind_bus(master) != AudioMixerCode::success
            || validation_mixer.bind_bus(master)
                != AudioMixerCode::duplicate_bus_identity
            || validation_mixer.bind_bus(LogicalBusBinding{
                .logical_id = { .domain = 41, .value = 99 },
                .runtime_bus = master.runtime_bus
            }) != AudioMixerCode::duplicate_bus_handle
            || !validation_mixer.bus_binding(master.runtime_bus)
            || *validation_mixer.bus_binding(master.runtime_bus) != master)
        {
            return fail(AudioMixerContractFailure::bus_identity_failed);
        }
        pass();

        const auto source = manager.create_source(source_descriptor(
            manager,
            primary_id,
            4));
        if (!source.succeeded()
            || !manager.submit(AudioCommandRequest{
                .target_frame = 1,
                .payload = PlayCommand{ .source = source.handle }
            }).accepted())
        {
            return fail(AudioMixerContractFailure::event_playback_failed);
        }
        const FrameMixPlan event_plan = prepare(manager, 1, 4);
        const AudioMixerSnapshot replay_point = validation_mixer.snapshot();
        const MixedAudioFrame first_mix = validation_mixer.mix(
            event_plan,
            mix_request(4));
        if (!exact_samples(first_mix, {
                0.0f, 0.0f,
                0.5f, 0.5f,
                1.0f, 1.0f,
                0.5f, 0.5f
            })
            || first_mix.logical_buses
                != std::vector<LogicalResourceId>{ master.logical_id }
            || first_mix.metrics.requested_frames != 4
            || first_mix.metrics.source_frames_sampled != 4)
        {
            return fail(AudioMixerContractFailure::event_playback_failed);
        }
        pass();

        if (validation_mixer.restore(replay_point) != AudioMixerCode::success)
            return fail(AudioMixerContractFailure::snapshot_restore_failed);
        const MixedAudioFrame replay_mix = validation_mixer.mix(
            event_plan,
            mix_request(4));
        if (replay_mix.interleaved_samples != first_mix.interleaved_samples
            || replay_mix.metrics != first_mix.metrics
            || validation_mixer.metrics().frames_produced != 4)
        {
            return fail(AudioMixerContractFailure::deterministic_replay_failed);
        }
        pass();

        AudioManager resample_manager{};
        AudioMixer resample_mixer{};
        const auto resample_master = master_binding(resample_manager);
        const ClipId resample_id{ .domain = 42, .value = 1 };
        if (resample_mixer.bind_bus(resample_master) != AudioMixerCode::success
            || resample_mixer.register_clip(
                mono_clip(resample_id, { 0.0f, 1.0f, 0.0f, -1.0f }))
                != AudioMixerCode::success)
        {
            return fail(AudioMixerContractFailure::resampling_failed);
        }
        auto resample_descriptor = source_descriptor(
            resample_manager,
            resample_id,
            4);
        resample_descriptor.pitch = 0.5f;
        const auto resample_source = resample_manager.create_source(
            resample_descriptor);
        if (!resample_source.succeeded()
            || !resample_manager.submit(AudioCommandRequest{
                .target_frame = 1,
                .payload = PlayCommand{ .source = resample_source.handle }
            }).accepted())
        {
            return fail(AudioMixerContractFailure::resampling_failed);
        }
        const auto resampled = resample_mixer.mix(
            prepare(resample_manager, 1, 4),
            mix_request(4));
        if (!exact_samples(resampled, {
                0.0f, 0.0f,
                0.5f, 0.5f,
                1.0f, 1.0f,
                0.5f, 0.5f
            })
            || resampled.metrics.interpolated_source_frames != 2)
        {
            return fail(AudioMixerContractFailure::resampling_failed);
        }
        pass();

        AudioManager loop_manager{};
        AudioMixer loop_mixer{};
        const auto loop_master = master_binding(loop_manager);
        const ClipId loop_id{ .domain = 43, .value = 1 };
        if (loop_mixer.bind_bus(loop_master) != AudioMixerCode::success
            || loop_mixer.register_clip(
                mono_clip(loop_id, { 0.25f, 0.5f, 0.75f, 1.0f }))
                != AudioMixerCode::success)
        {
            return fail(AudioMixerContractFailure::loop_boundary_failed);
        }
        const auto loop_source = loop_manager.create_source(source_descriptor(
            loop_manager,
            loop_id,
            4,
            1.0f,
            true));
        if (!loop_source.succeeded()
            || !loop_manager.submit(AudioCommandRequest{
                .target_frame = 1,
                .payload = PlayCommand{
                    .source = loop_source.handle,
                    .start_seconds = 3.0 / contract_sample_rate
                }
            }).accepted())
        {
            return fail(AudioMixerContractFailure::loop_boundary_failed);
        }
        const auto looped = loop_mixer.mix(
            prepare(loop_manager, 1, 6),
            mix_request(6));
        if (!exact_samples(looped, {
                1.0f, 1.0f,
                0.25f, 0.25f,
                0.5f, 0.5f,
                0.75f, 0.75f,
                1.0f, 1.0f,
                0.25f, 0.25f
            })
            || looped.metrics.loop_wraps != 2)
        {
            return fail(AudioMixerContractFailure::loop_boundary_failed);
        }
        pass();

        AudioManager control_manager{};
        AudioMixer control_mixer{};
        const auto control_master = master_binding(control_manager);
        const ClipId control_id{ .domain = 44, .value = 1 };
        if (control_mixer.bind_bus(control_master) != AudioMixerCode::success
            || control_mixer.register_clip(
                mono_clip(control_id, { 1.0f, 1.0f, 1.0f, 1.0f }))
                != AudioMixerCode::success)
        {
            return fail(AudioMixerContractFailure::bus_gain_failed);
        }
        const auto control_source = control_manager.create_source(
            source_descriptor(control_manager, control_id, 4, 0.5f, true));
        if (!control_source.succeeded()
            || !control_manager.submit(AudioCommandRequest{
                .target_frame = 1,
                .order_group = 0,
                .payload = SetBusGainCommand{
                    .bus = control_manager.master_bus(),
                    .gain = 0.5f
                }
            }).accepted()
            || !control_manager.submit(AudioCommandRequest{
                .target_frame = 1,
                .order_group = 1,
                .payload = PlayCommand{ .source = control_source.handle }
            }).accepted())
        {
            return fail(AudioMixerContractFailure::bus_gain_failed);
        }
        const auto gained = control_mixer.mix(
            prepare(control_manager, 1, 1),
            mix_request(1));
        if (!exact_samples(gained, { 0.25f, 0.25f }))
            return fail(AudioMixerContractFailure::bus_gain_failed);
        pass();

        if (!control_manager.submit(AudioCommandRequest{
                .target_frame = 2,
                .payload = SetBusMuteCommand{
                    .bus = control_manager.master_bus(),
                    .muted = true
                }
            }).accepted())
        {
            return fail(AudioMixerContractFailure::bus_mute_failed);
        }
        const auto muted = control_mixer.mix(
            prepare(control_manager, 2, 1),
            mix_request(1));
        if (!exact_samples(muted, { 0.0f, 0.0f })
            || muted.metrics.muted_sources != 1
            || muted.metrics.mixed_sources != 0)
        {
            return fail(AudioMixerContractFailure::bus_mute_failed);
        }
        pass();

        if (!control_manager.submit(AudioCommandRequest{
                .target_frame = 3,
                .payload = PauseCommand{ .source = control_source.handle }
            }).accepted())
        {
            return fail(AudioMixerContractFailure::pause_failed);
        }
        const auto paused = control_mixer.mix(
            prepare(control_manager, 3, 1),
            mix_request(1));
        if (!exact_samples(paused, { 0.0f, 0.0f })
            || paused.metrics.planned_sources != 0)
        {
            return fail(AudioMixerContractFailure::pause_failed);
        }
        pass();

        if (!control_manager.submit(AudioCommandRequest{
                .target_frame = 4,
                .order_group = 0,
                .payload = SetBusMuteCommand{
                    .bus = control_manager.master_bus(),
                    .muted = false
                }
            }).accepted()
            || !control_manager.submit(AudioCommandRequest{
                .target_frame = 4,
                .order_group = 1,
                .payload = ResumeCommand{ .source = control_source.handle }
            }).accepted()
            || !control_manager.submit(AudioCommandRequest{
                .target_frame = 4,
                .order_group = 2,
                .payload = StopCommand{
                    .source = control_source.handle,
                    .reset_cursor = true
                }
            }).accepted())
        {
            return fail(AudioMixerContractFailure::stop_failed);
        }
        const auto stopped = control_mixer.mix(
            prepare(control_manager, 4, 1),
            mix_request(1));
        if (!exact_samples(stopped, { 0.0f, 0.0f })
            || stopped.metrics.planned_sources != 0)
        {
            return fail(AudioMixerContractFailure::stop_failed);
        }
        pass();

        AudioMixer missing_clip_mixer{};
        if (missing_clip_mixer.bind_bus(master) != AudioMixerCode::success)
            return fail(AudioMixerContractFailure::missing_clip_not_refused);
        const auto missing_clip_result = missing_clip_mixer.mix(
            event_plan,
            mix_request(4));
        if (missing_clip_result.code != AudioMixerCode::missing_clip
            || !missing_clip_result.interleaved_samples.empty())
        {
            return fail(AudioMixerContractFailure::missing_clip_not_refused);
        }
        pass();

        AudioMixer missing_bus_mixer{};
        if (missing_bus_mixer.register_clip(
                mono_clip(primary_id, { 0.0f, 0.5f, 1.0f, 0.5f }))
                != AudioMixerCode::success)
        {
            return fail(AudioMixerContractFailure::missing_bus_not_refused);
        }
        const auto missing_bus_result = missing_bus_mixer.mix(
            event_plan,
            mix_request(4));
        if (missing_bus_result.code != AudioMixerCode::missing_bus
            || !missing_bus_result.interleaved_samples.empty())
        {
            return fail(AudioMixerContractFailure::missing_bus_not_refused);
        }
        pass();

        AudioMixer capacity_mixer{ AudioMixerLimits{
            .maximum_clips = 1,
            .maximum_bus_bindings = 1,
            .maximum_sources_per_mix = 1,
            .maximum_frames_per_clip = 4,
            .maximum_resident_samples = 4,
            .maximum_output_frames_per_mix = 4
        } };
        if (capacity_mixer.register_clip(
                mono_clip(primary_id, { 0.0f, 0.0f, 0.0f, 0.0f }))
                != AudioMixerCode::success
            || capacity_mixer.register_clip(mono_clip(second_id, { 0.0f }))
                != AudioMixerCode::capacity_exceeded
            || capacity_mixer.bind_bus(master) != AudioMixerCode::success
            || capacity_mixer.bind_bus(LogicalBusBinding{
                .logical_id = { .domain = 45, .value = 1 },
                .runtime_bus = { .index = 2, .generation = 1 }
            }) != AudioMixerCode::capacity_exceeded)
        {
            return fail(AudioMixerContractFailure::capacity_not_enforced);
        }
        pass();

        auto unsupported_request = mix_request(4);
        unsupported_request.output_format.channel_layout = PcmChannelLayout::mono;
        if (validation_mixer.mix(event_plan, unsupported_request).code
                != AudioMixerCode::unsupported_output_format
            || validation_mixer.mix(event_plan, mix_request(3)).code
                != AudioMixerCode::frame_duration_mismatch)
        {
            return fail(AudioMixerContractFailure::output_format_not_refused);
        }
        pass();

        const auto accounted = validation_mixer.mix(event_plan, mix_request(4));
        if (!accounted
            || accounted.frame_count() != 4
            || accounted.interleaved_samples.size() != 8
            || accounted.metrics.requested_frames
                != accounted.metrics.produced_frames)
        {
            return fail(AudioMixerContractFailure::frame_accounting_failed);
        }
        pass();

        const AudioMixerSnapshot before_registry_change =
            validation_mixer.snapshot();
        if (validation_mixer.register_clip(mono_clip(second_id, { 0.0f }))
                != AudioMixerCode::success
            || validation_mixer.restore(before_registry_change)
                != AudioMixerCode::snapshot_registry_mismatch)
        {
            return fail(AudioMixerContractFailure::snapshot_restore_failed);
        }
        pass();

        validation_mixer.reset();
        const auto reset_metrics = validation_mixer.metrics();
        if (!validation_mixer.contains_clip(primary_id)
            || !validation_mixer.bus_binding(master.runtime_bus)
            || reset_metrics.resident_clips != 2
            || reset_metrics.bound_buses != 1
            || reset_metrics.mix_requests != 0
            || reset_metrics.frames_produced != 0
            || reset_metrics.resets != 1)
        {
            return fail(AudioMixerContractFailure::reset_failed);
        }
        pass();

        validation_mixer.teardown();
        const auto teardown_metrics = validation_mixer.metrics();
        if (validation_mixer.contains_clip(primary_id)
            || validation_mixer.bus_binding(master.runtime_bus)
            || teardown_metrics.resident_clips != 0
            || teardown_metrics.bound_buses != 0
            || teardown_metrics.resident_samples != 0
            || teardown_metrics.resident_bytes != 0
            || teardown_metrics.mix_requests != 0)
        {
            return fail(AudioMixerContractFailure::teardown_failed);
        }
        pass();

        report.passed = true;
        report.failure = AudioMixerContractFailure::none;
        return report;
    }
}

namespace epochengine::audio::contract
{
    int run_audio_mixer_contract()
    {
        return run_audio_mixer_contract_tests().passed ? 0 : 1;
    }
}

#if defined(EPOCH_AUDIO_MIXER_CONTRACT_MAIN)
int main()
{
    return epochengine::audio::contract::run_audio_mixer_contract();
}
#endif
