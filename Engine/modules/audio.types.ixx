/*
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 *
 * Backend-neutral audio scheduling types. Physical decoding and output are
 * intentionally not represented as successful work in this foundation.
 */
module;

#include <compare>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

export module audio.types;

export namespace epochengine::audio
{
    inline constexpr std::uint32_t null_handle_index = 0;

    template <class Tag>
    struct GenerationHandle final
    {
        std::uint32_t index{};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return index != null_handle_index && generation != 0;
        }

        constexpr explicit operator bool() const noexcept
        {
            return valid();
        }

        constexpr auto operator<=>(const GenerationHandle&) const noexcept = default;
    };

    struct SourceHandleTag final {};
    struct BusHandleTag final {};

    using SourceHandle = GenerationHandle<SourceHandleTag>;
    using BusHandle = GenerationHandle<BusHandleTag>;

    inline constexpr SourceHandle null_source{};
    inline constexpr BusHandle null_bus{};

    struct LogicalResourceId final
    {
        std::uint64_t domain{};
        std::uint64_t value{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return domain != 0 || value != 0;
        }

        constexpr explicit operator bool() const noexcept
        {
            return valid();
        }

        constexpr auto operator<=>(const LogicalResourceId&) const noexcept = default;
    };

    using ClipId = LogicalResourceId;

    struct Vector3 final
    {
        float x{};
        float y{};
        float z{};

        constexpr auto operator<=>(const Vector3&) const noexcept = default;
    };

    enum class PlaybackState : std::uint8_t
    {
        stopped,
        scheduled,
        playing,
        paused
    };

    enum class PlaybackDirection : std::int8_t
    {
        reverse = -1,
        forward = 1
    };

    enum class SeekPolicy : std::uint8_t
    {
        exact_if_supported,
        clamp_to_clip,
        reject_if_unseekable
    };

    enum class ReversePlaybackPolicy : std::uint8_t
    {
        disabled,
        require_reverse_resource,
        logical_timeline_only
    };

    enum class PhysicalOutputState : std::uint8_t
    {
        unavailable_no_backend
    };

    struct TemporalPlaybackPolicy final
    {
        SeekPolicy seek{ SeekPolicy::exact_if_supported };
        ReversePlaybackPolicy reverse{ ReversePlaybackPolicy::disabled };
        bool preserve_pitch{ true };
    };

    struct ClipResource final
    {
        ClipId id{};
        double duration_seconds{};
        std::uint32_t nominal_sample_rate{ 48'000 };
        std::uint16_t channel_count{ 2 };
        bool seekable{ true };
        bool reverse_decodable{};
    };

    struct SpatialState final
    {
        Vector3 position{};
        Vector3 velocity{};
        float reference_distance{ 1.0f };
        float maximum_distance{ 10'000.0f };
        float rolloff{ 1.0f };
        float minimum_gain{};
        float maximum_gain{ 1.0f };
        bool enabled{};
    };

    struct ListenerState final
    {
        Vector3 position{};
        Vector3 velocity{};
        Vector3 forward{ 0.0f, 0.0f, -1.0f };
        Vector3 up{ 0.0f, 1.0f, 0.0f };
        float world_units_per_metre{ 1.0f };
    };

    struct SourceDescriptor final
    {
        ClipResource clip{};
        BusHandle bus{};
        SpatialState spatial{};
        TemporalPlaybackPolicy temporal{};
        float gain{ 1.0f };
        float pitch{ 1.0f };
        bool looping{};
    };

    struct BusDescriptor final
    {
        LogicalResourceId id{};
        BusHandle parent{};
        float gain{ 1.0f };
        bool muted{};
    };

    struct PlayCommand final
    {
        SourceHandle source{};
        double start_seconds{};
    };

    struct PauseCommand final
    {
        SourceHandle source{};
    };

    struct ResumeCommand final
    {
        SourceHandle source{};
    };

    struct StopCommand final
    {
        SourceHandle source{};
        bool reset_cursor{ true };
    };

    struct SeekCommand final
    {
        SourceHandle source{};
        double seconds{};
    };

    struct SetSourceGainCommand final
    {
        SourceHandle source{};
        float gain{ 1.0f };
    };

    struct SetSourcePitchCommand final
    {
        SourceHandle source{};
        float pitch{ 1.0f };
    };

    struct RouteSourceCommand final
    {
        SourceHandle source{};
        BusHandle bus{};
    };

    struct SetSpatialStateCommand final
    {
        SourceHandle source{};
        SpatialState spatial{};
    };

    struct SetPlaybackDirectionCommand final
    {
        SourceHandle source{};
        PlaybackDirection direction{ PlaybackDirection::forward };
    };

    struct SetTemporalPolicyCommand final
    {
        SourceHandle source{};
        TemporalPlaybackPolicy policy{};
    };

    struct SetBusGainCommand final
    {
        BusHandle bus{};
        float gain{ 1.0f };
    };

    struct SetBusMuteCommand final
    {
        BusHandle bus{};
        bool muted{};
    };

    struct SetListenerStateCommand final
    {
        ListenerState listener{};
    };

    using AudioCommandPayload = std::variant<
        PlayCommand,
        PauseCommand,
        ResumeCommand,
        StopCommand,
        SeekCommand,
        SetSourceGainCommand,
        SetSourcePitchCommand,
        RouteSourceCommand,
        SetSpatialStateCommand,
        SetPlaybackDirectionCommand,
        SetTemporalPolicyCommand,
        SetBusGainCommand,
        SetBusMuteCommand,
        SetListenerStateCommand>;

    enum class AudioCommandKind : std::uint8_t
    {
        play,
        pause,
        resume,
        stop,
        seek,
        set_source_gain,
        set_source_pitch,
        route_source,
        set_spatial_state,
        set_playback_direction,
        set_temporal_policy,
        set_bus_gain,
        set_bus_mute,
        set_listener_state
    };

    struct AudioCommandRequest final
    {
        std::uint64_t target_frame{};
        std::uint32_t order_group{};
        AudioCommandPayload payload{};
    };

    enum class ScheduleStatus : std::uint8_t
    {
        accepted_logical,
        rejected
    };

    enum class ScheduleRejectReason : std::uint8_t
    {
        none,
        queue_capacity_reached,
        stale_source_handle,
        stale_bus_handle,
        invalid_value,
        invalid_temporal_policy,
        frame_already_compiled
    };

    struct CommandReceipt final
    {
        ScheduleStatus status{ ScheduleStatus::rejected };
        ScheduleRejectReason reason{ ScheduleRejectReason::none };
        std::uint64_t sequence{};
        std::uint64_t target_frame{};

        [[nodiscard]] constexpr bool accepted() const noexcept
        {
            return status == ScheduleStatus::accepted_logical;
        }
    };

    enum class RegistryStatus : std::uint8_t
    {
        success,
        capacity_reached,
        invalid_descriptor,
        stale_handle,
        resource_in_use,
        reserved_resource
    };

    template <class HandleType>
    struct HandleResult final
    {
        RegistryStatus status{ RegistryStatus::invalid_descriptor };
        HandleType handle{};

        [[nodiscard]] constexpr bool succeeded() const noexcept
        {
            return status == RegistryStatus::success && handle.valid();
        }
    };

    struct FrameRequest final
    {
        std::uint64_t frame_index{};
        double timeline_seconds{};
        double duration_seconds{ 1.0 / 60.0 };
    };

    enum class FramePlanStatus : std::uint8_t
    {
        ready_logical_only,
        rejected_non_monotonic_frame,
        rejected_invalid_timing
    };

    enum class CommandApplyStatus : std::uint8_t
    {
        applied,
        ignored_for_current_state,
        dropped_stale_target
    };

    struct AppliedCommand final
    {
        std::uint64_t sequence{};
        std::uint64_t target_frame{};
        std::uint32_t order_group{};
        AudioCommandKind kind{ AudioCommandKind::play };
        CommandApplyStatus status{ CommandApplyStatus::applied };
    };

    struct SourceMixPlan final
    {
        SourceHandle source{};
        ClipId clip{};
        BusHandle bus{};
        PlaybackDirection direction{ PlaybackDirection::forward };
        SpatialState spatial{};
        double cursor_begin_seconds{};
        double cursor_end_seconds{};
        float source_gain{ 1.0f };
        float effective_bus_gain{ 1.0f };
        float effective_gain{ 1.0f };
        float pitch{ 1.0f };
        bool logically_audible{};
        bool looping{};
    };

    struct FrameMixMetrics final
    {
        std::size_t commands_considered{};
        std::size_t commands_applied{};
        std::size_t commands_ignored{};
        std::size_t stale_commands_dropped{};
        std::size_t pending_commands{};
        std::size_t registered_sources{};
        std::size_t active_sources{};
        std::size_t planned_sources{};
        std::size_t logically_audible_sources{};
        std::size_t muted_sources{};
        std::size_t registered_buses{};
        std::uint64_t physical_frames_written{};
        bool plan_capacity_limited{};
    };

    struct FrameMixPlan final
    {
        FramePlanStatus status{ FramePlanStatus::ready_logical_only };
        PhysicalOutputState physical_output{
            PhysicalOutputState::unavailable_no_backend
        };
        std::uint64_t frame_index{};
        double timeline_seconds{};
        double duration_seconds{};
        ListenerState listener{};
        std::vector<AppliedCommand> applied_commands{};
        std::vector<SourceMixPlan> sources{};
        FrameMixMetrics metrics{};

        [[nodiscard]] constexpr bool logical_schedule_ready() const noexcept
        {
            return status == FramePlanStatus::ready_logical_only;
        }

        [[nodiscard]] constexpr bool physical_output_available() const noexcept
        {
            return false;
        }
    };

    struct SourceSnapshot final
    {
        SourceHandle handle{};
        ClipResource clip{};
        BusHandle bus{};
        SpatialState spatial{};
        TemporalPlaybackPolicy temporal{};
        PlaybackState state{ PlaybackState::stopped };
        PlaybackDirection direction{ PlaybackDirection::forward };
        double cursor_seconds{};
        float gain{ 1.0f };
        float pitch{ 1.0f };
        bool looping{};
    };

    struct BusSnapshot final
    {
        BusHandle handle{};
        LogicalResourceId id{};
        BusHandle parent{};
        float gain{ 1.0f };
        float effective_gain{ 1.0f };
        bool muted{};
        bool effectively_muted{};
    };

    struct AudioManagerMetrics final
    {
        std::size_t registered_sources{};
        std::size_t registered_buses{};
        std::size_t pending_commands{};
        std::uint64_t commands_accepted{};
        std::uint64_t commands_rejected{};
        std::uint64_t stale_commands_dropped{};
        std::uint64_t plans_compiled{};
        std::uint64_t non_monotonic_plans_rejected{};
        PhysicalOutputState physical_output{
            PhysicalOutputState::unavailable_no_backend
        };
    };

    enum class AudioContractFailure : std::uint8_t
    {
        none,
        master_bus_missing,
        source_creation_failed,
        stale_source_accepted,
        generation_not_advanced,
        command_order_not_deterministic,
        logical_plan_missing,
        physical_output_claimed,
        bounds_not_enforced,
        reverse_policy_not_enforced,
        seek_policy_not_enforced,
        virtual_source_cursor_frozen
    };

    struct AudioContractReport final
    {
        bool passed{};
        std::uint32_t checks_completed{};
        AudioContractFailure failure{ AudioContractFailure::none };
    };
}
