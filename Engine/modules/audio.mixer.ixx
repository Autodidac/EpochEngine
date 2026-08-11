/*
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 *
 * Deterministic renderer-independent PCM mixing over audio.manager plans.
 * This module owns bounded decoded runtime clips, but never an OS device or
 * canonical authoring audio.
 */
module;

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

export module audio.mixer;

export import audio.manager;

export namespace epochengine::audio
{
    enum class PcmSampleFormat : std::uint8_t
    {
        float32_interleaved,
        signed16_interleaved,
        float32_planar
    };

    enum class PcmChannelLayout : std::uint8_t
    {
        mono,
        stereo,
        surround_5_1
    };

    struct PcmFormat final
    {
        PcmSampleFormat sample_format{ PcmSampleFormat::float32_interleaved };
        PcmChannelLayout channel_layout{ PcmChannelLayout::stereo };
        std::uint32_t sample_rate{ 48'000 };

        [[nodiscard]] friend constexpr bool operator==(
            const PcmFormat&,
            const PcmFormat&) noexcept = default;
    };

    struct AudioOutputFormat final
    {
        PcmSampleFormat sample_format{ PcmSampleFormat::float32_interleaved };
        PcmChannelLayout channel_layout{ PcmChannelLayout::stereo };
        std::uint32_t sample_rate{ 48'000 };

        [[nodiscard]] friend constexpr bool operator==(
            const AudioOutputFormat&,
            const AudioOutputFormat&) noexcept = default;
    };

    struct OwnedPcmClip final
    {
        ClipId id{};
        PcmFormat format{};
        std::uint64_t frame_count{};
        std::vector<float> interleaved_samples{};
    };

    struct PcmClipDescriptor final
    {
        ClipId id{};
        PcmFormat format{};
        std::uint64_t frame_count{};
        std::uint64_t sample_count{};
        std::uint64_t content_digest{};

        [[nodiscard]] friend constexpr bool operator==(
            const PcmClipDescriptor&,
            const PcmClipDescriptor&) noexcept = default;
    };

    struct LogicalBusBinding final
    {
        LogicalResourceId logical_id{};
        BusHandle runtime_bus{};

        [[nodiscard]] friend constexpr bool operator==(
            const LogicalBusBinding&,
            const LogicalBusBinding&) noexcept = default;
    };

    struct AudioMixerLimits final
    {
        std::uint32_t maximum_clips{ 1'024 };
        std::uint32_t maximum_bus_bindings{ 64 };
        std::uint32_t maximum_sources_per_mix{ 1'024 };
        std::uint64_t maximum_frames_per_clip{ 16'777'216 };
        std::uint64_t maximum_resident_samples{ 33'554'432 };
        std::uint32_t maximum_output_frames_per_mix{ 480'000 };

        [[nodiscard]] friend constexpr bool operator==(
            const AudioMixerLimits&,
            const AudioMixerLimits&) noexcept = default;
    };

    enum class AudioMixerCode : std::uint8_t
    {
        success,
        duplicate_clip,
        missing_clip,
        duplicate_bus_identity,
        duplicate_bus_handle,
        missing_bus,
        capacity_exceeded,
        sample_budget_exceeded,
        malformed_clip,
        unsupported_clip_format,
        unsupported_output_format,
        invalid_output_frame_count,
        invalid_frame_plan,
        frame_duration_mismatch,
        invalid_snapshot,
        snapshot_registry_mismatch
    };

    struct AudioMixRequest final
    {
        AudioOutputFormat output_format{};
        std::uint32_t output_frame_count{};
    };

    struct AudioMixFrameMetrics final
    {
        std::uint32_t requested_frames{};
        std::uint32_t produced_frames{};
        std::uint32_t planned_sources{};
        std::uint32_t mixed_sources{};
        std::uint32_t muted_sources{};
        std::uint32_t logical_buses_used{};
        std::uint64_t source_frames_sampled{};
        std::uint64_t interpolated_source_frames{};
        std::uint64_t loop_wraps{};
        std::uint64_t clipped_output_samples{};

        [[nodiscard]] friend constexpr bool operator==(
            const AudioMixFrameMetrics&,
            const AudioMixFrameMetrics&) noexcept = default;
    };

    struct MixedAudioFrame final
    {
        AudioMixerCode code{ AudioMixerCode::invalid_frame_plan };
        AudioOutputFormat format{};
        std::uint64_t plan_frame_index{};
        std::vector<float> interleaved_samples{};
        std::vector<LogicalResourceId> logical_buses{};
        AudioMixFrameMetrics metrics{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == AudioMixerCode::success;
        }

        [[nodiscard]] constexpr std::uint32_t frame_count() const noexcept
        {
            return metrics.produced_frames;
        }
    };

    struct AudioMixerMetrics final
    {
        std::uint32_t resident_clips{};
        std::uint32_t bound_buses{};
        std::uint64_t resident_frames{};
        std::uint64_t resident_samples{};
        std::uint64_t resident_bytes{};
        std::uint64_t peak_resident_samples{};
        std::uint64_t mix_requests{};
        std::uint64_t rejected_mix_requests{};
        std::uint64_t frames_produced{};
        std::uint64_t source_frames_sampled{};
        std::uint64_t clipped_output_samples{};
        std::uint64_t resets{};

        [[nodiscard]] friend constexpr bool operator==(
            const AudioMixerMetrics&,
            const AudioMixerMetrics&) noexcept = default;
    };

    struct AudioMixerSnapshot final
    {
        std::vector<PcmClipDescriptor> clips{};
        std::vector<LogicalBusBinding> buses{};
        AudioMixerMetrics metrics{};

        [[nodiscard]] friend bool operator==(
            const AudioMixerSnapshot&,
            const AudioMixerSnapshot&) = default;
    };

    class AudioMixer final
    {
    public:
        /*
         * AudioManager remains the sole owner of source identity, commands,
         * cursors, loops, and bus controls. AudioMixer only resolves immutable
         * PCM resources and renders an accepted FrameMixPlan into an explicit
         * caller-selected output format.
         */
        explicit AudioMixer(AudioMixerLimits limits = {});
        ~AudioMixer();

        AudioMixer(AudioMixer&&) noexcept;
        AudioMixer& operator=(AudioMixer&&) noexcept;

        AudioMixer(const AudioMixer&) = delete;
        AudioMixer& operator=(const AudioMixer&) = delete;

        [[nodiscard]] const AudioMixerLimits& limits() const noexcept;

        [[nodiscard]] AudioMixerCode register_clip(OwnedPcmClip clip);
        [[nodiscard]] AudioMixerCode unregister_clip(ClipId clip);
        [[nodiscard]] bool contains_clip(ClipId clip) const noexcept;
        [[nodiscard]] std::optional<PcmClipDescriptor> clip_descriptor(
            ClipId clip) const;

        [[nodiscard]] AudioMixerCode bind_bus(
            const LogicalBusBinding& binding);
        [[nodiscard]] AudioMixerCode unbind_bus(LogicalResourceId logical_bus);
        [[nodiscard]] std::optional<LogicalBusBinding> bus_binding(
            BusHandle runtime_bus) const;

        [[nodiscard]] MixedAudioFrame mix(
            const FrameMixPlan& plan,
            const AudioMixRequest& request);

        [[nodiscard]] AudioMixerSnapshot snapshot() const;
        [[nodiscard]] AudioMixerCode restore(
            const AudioMixerSnapshot& snapshot);

        void reset() noexcept;
        void teardown() noexcept;
        [[nodiscard]] AudioMixerMetrics metrics() const noexcept;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_;
    };

    enum class AudioMixerContractFailure : std::uint8_t
    {
        none,
        clip_validation_failed,
        bus_identity_failed,
        event_playback_failed,
        deterministic_replay_failed,
        resampling_failed,
        loop_boundary_failed,
        bus_gain_failed,
        bus_mute_failed,
        pause_failed,
        stop_failed,
        missing_clip_not_refused,
        missing_bus_not_refused,
        capacity_not_enforced,
        output_format_not_refused,
        frame_accounting_failed,
        snapshot_restore_failed,
        reset_failed,
        teardown_failed
    };

    struct AudioMixerContractReport final
    {
        bool passed{};
        std::uint32_t checks_completed{};
        AudioMixerContractFailure failure{
            AudioMixerContractFailure::none
        };
    };

    [[nodiscard]] AudioMixerContractReport run_audio_mixer_contract_tests();
}

export namespace epochengine::audio::contract
{
    [[nodiscard]] int run_audio_mixer_contract();
}
