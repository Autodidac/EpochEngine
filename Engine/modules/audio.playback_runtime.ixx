/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <compare>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

export module audio.playback_runtime;

export import audio.device;
export import audio.mixer;

export namespace epochengine::audio
{
    struct PlaybackSessionHandle final
    {
        std::uint64_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return generation != 0;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return valid();
        }

        [[nodiscard]] friend constexpr auto operator<=>(
            const PlaybackSessionHandle&,
            const PlaybackSessionHandle&) noexcept = default;
    };

    struct PlaybackBusDefinition final
    {
        LogicalResourceId id{};
        LogicalResourceId parent{};
        float gain{1.0f};
        bool muted{};
    };

    struct PlaybackCueDefinition final
    {
        OwnedPcmClip clip{};
        LogicalResourceId bus{};
        float gain{1.0f};
        bool looping{};
    };

    struct PlaybackSessionRequest final
    {
        std::uint64_t stable_session_id{};
        std::vector<PlaybackBusDefinition> buses{};
        std::vector<PlaybackCueDefinition> cues{};
        bool request_physical_output{true};
    };

    enum class PlaybackRuntimeCode : std::uint8_t
    {
        success,
        busy,
        invalid_session,
        invalid_request,
        duplicate_cue,
        clip_rejected,
        bus_rejected,
        source_rejected,
        command_rejected,
        frame_rejected,
        mix_rejected,
        physical_queue_saturated,
        physical_output_failed
    };

    struct PlaybackSessionResult final
    {
        PlaybackRuntimeCode code{PlaybackRuntimeCode::invalid_request};
        PlaybackSessionHandle handle{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == PlaybackRuntimeCode::success && handle.valid();
        }
    };

    struct PlaybackAdvanceResult final
    {
        PlaybackRuntimeCode code{PlaybackRuntimeCode::invalid_session};
        std::uint64_t frame_index{};
        double timeline_seconds{};
        MixedAudioFrame mixed{};
        AudioDeviceState physical_state{AudioDeviceState::unavailable};
        AudioDeviceCode physical_code{AudioDeviceCode::unavailable};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == PlaybackRuntimeCode::success
                || code == PlaybackRuntimeCode::physical_queue_saturated;
        }
    };

    struct PlaybackRuntimeMetrics final
    {
        std::uint64_t sessions_opened{};
        std::uint64_t sessions_closed{};
        std::uint64_t cues_registered{};
        std::uint64_t buses_registered{};
        std::uint64_t bus_control_transitions{};
        std::uint64_t triggers_accepted{};
        std::uint64_t triggers_rejected{};
        std::uint64_t frames_mixed{};
        std::uint64_t frames_submitted{};
        std::uint64_t physical_failures{};
        std::uint64_t resets{};
        std::uint64_t pause_transitions{};

        [[nodiscard]] friend constexpr bool operator==(
            const PlaybackRuntimeMetrics&,
            const PlaybackRuntimeMetrics&) noexcept = default;
    };

    struct PlaybackRuntimeSnapshot final
    {
        bool session_active{};
        bool paused{};
        std::uint64_t stable_session_id{};
        PlaybackSessionHandle handle{};
        std::uint64_t frame_index{};
        double timeline_seconds{};
        std::vector<ClipId> cues{};
        std::vector<LogicalResourceId> buses{};
        PlaybackRuntimeMetrics metrics{};
        AudioMixerMetrics mixer{};
        AudioDeviceSnapshot device{};
        std::string diagnostic{};
    };

    class PlaybackRuntime final
    {
    public:
        explicit PlaybackRuntime(
            std::unique_ptr<AudioDeviceSink> physicalSink = {},
            AudioMixerLimits mixerLimits = {});
        ~PlaybackRuntime();

        PlaybackRuntime(PlaybackRuntime&&) noexcept;
        PlaybackRuntime& operator=(PlaybackRuntime&&) noexcept;

        PlaybackRuntime(const PlaybackRuntime&) = delete;
        PlaybackRuntime& operator=(const PlaybackRuntime&) = delete;

        [[nodiscard]] PlaybackSessionResult open_session(
            PlaybackSessionRequest request);
        [[nodiscard]] PlaybackRuntimeCode trigger(
            PlaybackSessionHandle session,
            ClipId cue,
            double startSeconds = 0.0);
        [[nodiscard]] PlaybackRuntimeCode set_bus_gain(
            PlaybackSessionHandle session,
            LogicalResourceId bus,
            float gain);
        [[nodiscard]] PlaybackRuntimeCode set_bus_muted(
            PlaybackSessionHandle session,
            LogicalResourceId bus,
            bool muted);
        [[nodiscard]] PlaybackRuntimeCode set_paused(
            PlaybackSessionHandle session,
            bool paused);
        [[nodiscard]] PlaybackRuntimeCode reset(
            PlaybackSessionHandle session);
        [[nodiscard]] PlaybackAdvanceResult advance(
            PlaybackSessionHandle session,
            double durationSeconds);
        [[nodiscard]] PlaybackRuntimeCode close_session(
            PlaybackSessionHandle session) noexcept;

        [[nodiscard]] PlaybackRuntimeSnapshot snapshot() const;
        [[nodiscard]] PlaybackRuntimeMetrics metrics() const noexcept;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_;
    };

    [[nodiscard]] constexpr std::string_view playback_runtime_code_name(
        PlaybackRuntimeCode code) noexcept
    {
        switch (code)
        {
        case PlaybackRuntimeCode::success: return "success";
        case PlaybackRuntimeCode::busy: return "busy";
        case PlaybackRuntimeCode::invalid_session: return "invalid session";
        case PlaybackRuntimeCode::invalid_request: return "invalid request";
        case PlaybackRuntimeCode::duplicate_cue: return "duplicate cue";
        case PlaybackRuntimeCode::clip_rejected: return "clip rejected";
        case PlaybackRuntimeCode::bus_rejected: return "bus rejected";
        case PlaybackRuntimeCode::source_rejected: return "source rejected";
        case PlaybackRuntimeCode::command_rejected: return "command rejected";
        case PlaybackRuntimeCode::frame_rejected: return "frame rejected";
        case PlaybackRuntimeCode::mix_rejected: return "mix rejected";
        case PlaybackRuntimeCode::physical_queue_saturated: return "physical queue saturated";
        case PlaybackRuntimeCode::physical_output_failed: return "physical output failed";
        }
        return "unknown";
    }

    [[nodiscard]] int run_audio_playback_runtime_contract_tests();
}
