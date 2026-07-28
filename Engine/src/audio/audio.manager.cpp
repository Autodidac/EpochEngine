/*
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

module audio.manager;

namespace epochengine::audio
{
    namespace
    {
        constexpr std::uint32_t hard_maximum_sources = 65'536;
        constexpr std::uint32_t hard_maximum_buses = 4'096;
        constexpr std::uint32_t hard_maximum_pending_commands = 262'144;
        constexpr std::uint32_t hard_maximum_sources_per_plan = 65'536;
        constexpr std::uint32_t maximum_bus_depth = 64;

        [[nodiscard]] constexpr std::uint32_t next_generation(
            std::uint32_t generation) noexcept
        {
            ++generation;
            return generation == 0 ? 1 : generation;
        }

        [[nodiscard]] bool finite(double value) noexcept
        {
            return std::isfinite(value);
        }

        [[nodiscard]] bool finite(float value) noexcept
        {
            return std::isfinite(value);
        }

        [[nodiscard]] bool finite(const Vector3& value) noexcept
        {
            return finite(value.x) && finite(value.y) && finite(value.z);
        }

        [[nodiscard]] bool valid_gain(float gain) noexcept
        {
            return finite(gain) && gain >= 0.0f && gain <= 16.0f;
        }

        [[nodiscard]] bool valid_pitch(float pitch) noexcept
        {
            return finite(pitch) && pitch >= 0.125f && pitch <= 8.0f;
        }

        [[nodiscard]] bool valid_spatial_state(const SpatialState& state) noexcept
        {
            return finite(state.position)
                && finite(state.velocity)
                && finite(state.reference_distance)
                && finite(state.maximum_distance)
                && finite(state.rolloff)
                && finite(state.minimum_gain)
                && finite(state.maximum_gain)
                && state.reference_distance > 0.0f
                && state.maximum_distance >= state.reference_distance
                && state.rolloff >= 0.0f
                && state.minimum_gain >= 0.0f
                && state.maximum_gain >= state.minimum_gain
                && state.maximum_gain <= 16.0f;
        }

        [[nodiscard]] bool valid_listener_state(const ListenerState& state) noexcept
        {
            return finite(state.position)
                && finite(state.velocity)
                && finite(state.forward)
                && finite(state.up)
                && finite(state.world_units_per_metre)
                && state.world_units_per_metre > 0.0f;
        }

        [[nodiscard]] bool valid_clip(const ClipResource& clip) noexcept
        {
            return clip.id.valid()
                && finite(clip.duration_seconds)
                && clip.duration_seconds > 0.0
                && clip.nominal_sample_rate >= 8'000
                && clip.nominal_sample_rate <= 768'000
                && clip.channel_count > 0
                && clip.channel_count <= 64;
        }

        [[nodiscard]] bool valid_temporal_policy(
            const ClipResource& clip,
            const TemporalPlaybackPolicy& policy) noexcept
        {
            if (policy.reverse == ReversePlaybackPolicy::require_reverse_resource
                && !clip.reverse_decodable)
            {
                return false;
            }

            return true;
        }

        [[nodiscard]] AudioManagerLimits sanitize_limits(
            AudioManagerLimits limits) noexcept
        {
            limits.maximum_sources = (std::clamp)(
                limits.maximum_sources,
                1u,
                hard_maximum_sources);
            limits.maximum_buses = (std::clamp)(
                limits.maximum_buses,
                1u,
                hard_maximum_buses);
            limits.maximum_pending_commands = (std::clamp)(
                limits.maximum_pending_commands,
                1u,
                hard_maximum_pending_commands);
            limits.maximum_sources_per_plan = (std::clamp)(
                limits.maximum_sources_per_plan,
                1u,
                hard_maximum_sources_per_plan);
            return limits;
        }

        [[nodiscard]] double clamp_cursor(
            const ClipResource& clip,
            double seconds) noexcept
        {
            return (std::clamp)(seconds, 0.0, clip.duration_seconds);
        }

        [[nodiscard]] double wrap_cursor(
            const ClipResource& clip,
            double seconds) noexcept
        {
            const double duration = clip.duration_seconds;
            if (duration <= 0.0)
                return 0.0;

            double wrapped = std::fmod(seconds, duration);
            if (wrapped < 0.0)
                wrapped += duration;
            return wrapped;
        }
    }

    struct AudioManager::Implementation final
    {
        struct SourceSlot final
        {
            std::uint32_t generation{ 1 };
            bool alive{};
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

        struct BusSlot final
        {
            std::uint32_t generation{ 1 };
            bool alive{};
            LogicalResourceId id{};
            BusHandle parent{};
            float gain{ 1.0f };
            bool muted{};
        };

        struct PendingCommand final
        {
            std::uint64_t sequence{};
            std::uint64_t target_frame{};
            std::uint32_t order_group{};
            AudioCommandPayload payload{};
        };

        explicit Implementation(AudioManagerLimits requested_limits)
            : limits{ sanitize_limits(requested_limits) }
        {
            sources.reserve(static_cast<std::size_t>(limits.maximum_sources) + 1);
            buses.reserve(static_cast<std::size_t>(limits.maximum_buses) + 1);
            free_sources.reserve(limits.maximum_sources);
            free_buses.reserve(limits.maximum_buses);
            pending.reserve(limits.maximum_pending_commands);

            sources.emplace_back();
            buses.emplace_back();
            buses.push_back(BusSlot{
                .generation = 1,
                .alive = true,
                .id = LogicalResourceId{ .domain = 1, .value = 1 },
                .parent = null_bus,
                .gain = 1.0f,
                .muted = false
            });
            master = BusHandle{ .index = 1, .generation = 1 };
            registered_bus_count = 1;
        }

        [[nodiscard]] bool valid(SourceHandle handle) const noexcept
        {
            return handle.valid()
                && handle.index < sources.size()
                && sources[handle.index].alive
                && sources[handle.index].generation == handle.generation;
        }

        [[nodiscard]] bool valid(BusHandle handle) const noexcept
        {
            return handle.valid()
                && handle.index < buses.size()
                && buses[handle.index].alive
                && buses[handle.index].generation == handle.generation;
        }

        [[nodiscard]] SourceHandle source_handle(
            std::uint32_t index) const noexcept
        {
            return SourceHandle{
                .index = index,
                .generation = sources[index].generation
            };
        }

        [[nodiscard]] BusHandle bus_handle(std::uint32_t index) const noexcept
        {
            return BusHandle{
                .index = index,
                .generation = buses[index].generation
            };
        }

        [[nodiscard]] bool bus_parent_chain_valid(
            BusHandle child,
            BusHandle parent) const noexcept
        {
            if (!valid(parent))
                return false;

            BusHandle current = parent;
            for (std::uint32_t depth = 0; depth < maximum_bus_depth; ++depth)
            {
                if (current == child)
                    return false;

                const auto& slot = buses[current.index];
                if (!slot.parent.valid())
                    return true;
                if (!valid(slot.parent))
                    return false;
                current = slot.parent;
            }
            return false;
        }

        struct BusMixState final
        {
            float gain{ 1.0f };
            bool muted{};
        };

        [[nodiscard]] BusMixState effective_bus_state(
            BusHandle bus) const noexcept
        {
            BusMixState result{};
            BusHandle current = bus;

            for (std::uint32_t depth = 0; depth < maximum_bus_depth; ++depth)
            {
                if (!valid(current))
                    return BusMixState{ .gain = 0.0f, .muted = true };

                const auto& slot = buses[current.index];
                result.gain *= slot.gain;
                result.muted = result.muted || slot.muted;
                if (!slot.parent.valid())
                    return result;
                current = slot.parent;
            }

            return BusMixState{ .gain = 0.0f, .muted = true };
        }

        [[nodiscard]] ScheduleRejectReason validate_command(
            const AudioCommandRequest& request) const noexcept
        {
            if (last_compiled_frame
                && request.target_frame <= *last_compiled_frame)
            {
                return ScheduleRejectReason::frame_already_compiled;
            }

            return std::visit(
                [this](const auto& command) -> ScheduleRejectReason
                {
                    using Command = std::decay_t<decltype(command)>;

                    if constexpr (
                        std::is_same_v<Command, PlayCommand>
                        || std::is_same_v<Command, PauseCommand>
                        || std::is_same_v<Command, ResumeCommand>
                        || std::is_same_v<Command, StopCommand>)
                    {
                        if (!valid(command.source))
                            return ScheduleRejectReason::stale_source_handle;

                        if constexpr (std::is_same_v<Command, PlayCommand>)
                        {
                            if (!finite(command.start_seconds)
                                || command.start_seconds < 0.0)
                            {
                                return ScheduleRejectReason::invalid_value;
                            }
                        }
                    }
                    else if constexpr (std::is_same_v<Command, SeekCommand>)
                    {
                        if (!valid(command.source))
                            return ScheduleRejectReason::stale_source_handle;
                        if (!finite(command.seconds) || command.seconds < 0.0)
                            return ScheduleRejectReason::invalid_value;

                        const auto& source = sources[command.source.index];
                        if (!source.clip.seekable
                            && source.temporal.seek != SeekPolicy::clamp_to_clip)
                        {
                            return ScheduleRejectReason::invalid_temporal_policy;
                        }
                    }
                    else if constexpr (
                        std::is_same_v<Command, SetSourceGainCommand>)
                    {
                        if (!valid(command.source))
                            return ScheduleRejectReason::stale_source_handle;
                        if (!valid_gain(command.gain))
                            return ScheduleRejectReason::invalid_value;
                    }
                    else if constexpr (
                        std::is_same_v<Command, SetSourcePitchCommand>)
                    {
                        if (!valid(command.source))
                            return ScheduleRejectReason::stale_source_handle;
                        if (!valid_pitch(command.pitch))
                            return ScheduleRejectReason::invalid_value;
                    }
                    else if constexpr (std::is_same_v<Command, RouteSourceCommand>)
                    {
                        if (!valid(command.source))
                            return ScheduleRejectReason::stale_source_handle;
                        if (!valid(command.bus))
                            return ScheduleRejectReason::stale_bus_handle;
                    }
                    else if constexpr (
                        std::is_same_v<Command, SetSpatialStateCommand>)
                    {
                        if (!valid(command.source))
                            return ScheduleRejectReason::stale_source_handle;
                        if (!valid_spatial_state(command.spatial))
                            return ScheduleRejectReason::invalid_value;
                    }
                    else if constexpr (
                        std::is_same_v<Command, SetPlaybackDirectionCommand>)
                    {
                        if (!valid(command.source))
                            return ScheduleRejectReason::stale_source_handle;

                        const auto& source = sources[command.source.index];
                        if (command.direction == PlaybackDirection::reverse)
                        {
                            if (source.temporal.reverse
                                == ReversePlaybackPolicy::disabled)
                            {
                                return ScheduleRejectReason::invalid_temporal_policy;
                            }
                            if (source.temporal.reverse
                                    == ReversePlaybackPolicy::require_reverse_resource
                                && !source.clip.reverse_decodable)
                            {
                                return ScheduleRejectReason::invalid_temporal_policy;
                            }
                        }
                    }
                    else if constexpr (
                        std::is_same_v<Command, SetTemporalPolicyCommand>)
                    {
                        if (!valid(command.source))
                            return ScheduleRejectReason::stale_source_handle;
                        if (!valid_temporal_policy(
                                sources[command.source.index].clip,
                                command.policy))
                        {
                            return ScheduleRejectReason::invalid_temporal_policy;
                        }
                    }
                    else if constexpr (std::is_same_v<Command, SetBusGainCommand>)
                    {
                        if (!valid(command.bus))
                            return ScheduleRejectReason::stale_bus_handle;
                        if (!valid_gain(command.gain))
                            return ScheduleRejectReason::invalid_value;
                    }
                    else if constexpr (std::is_same_v<Command, SetBusMuteCommand>)
                    {
                        if (!valid(command.bus))
                            return ScheduleRejectReason::stale_bus_handle;
                    }
                    else if constexpr (
                        std::is_same_v<Command, SetListenerStateCommand>)
                    {
                        if (!valid_listener_state(command.listener))
                            return ScheduleRejectReason::invalid_value;
                    }

                    return ScheduleRejectReason::none;
                },
                request.payload);
        }

        [[nodiscard]] CommandApplyStatus apply_command(
            const PendingCommand& pending_command)
        {
            return std::visit(
                [this](const auto& command) -> CommandApplyStatus
                {
                    using Command = std::decay_t<decltype(command)>;

                    if constexpr (std::is_same_v<Command, PlayCommand>)
                    {
                        if (!valid(command.source))
                            return CommandApplyStatus::dropped_stale_target;
                        auto& source = sources[command.source.index];
                        source.cursor_seconds = source.looping
                            ? wrap_cursor(source.clip, command.start_seconds)
                            : clamp_cursor(source.clip, command.start_seconds);
                        source.state = PlaybackState::playing;
                    }
                    else if constexpr (std::is_same_v<Command, PauseCommand>)
                    {
                        if (!valid(command.source))
                            return CommandApplyStatus::dropped_stale_target;
                        auto& source = sources[command.source.index];
                        if (source.state != PlaybackState::playing)
                            return CommandApplyStatus::ignored_for_current_state;
                        source.state = PlaybackState::paused;
                    }
                    else if constexpr (std::is_same_v<Command, ResumeCommand>)
                    {
                        if (!valid(command.source))
                            return CommandApplyStatus::dropped_stale_target;
                        auto& source = sources[command.source.index];
                        if (source.state != PlaybackState::paused)
                            return CommandApplyStatus::ignored_for_current_state;
                        source.state = PlaybackState::playing;
                    }
                    else if constexpr (std::is_same_v<Command, StopCommand>)
                    {
                        if (!valid(command.source))
                            return CommandApplyStatus::dropped_stale_target;
                        auto& source = sources[command.source.index];
                        source.state = PlaybackState::stopped;
                        if (command.reset_cursor)
                            source.cursor_seconds = 0.0;
                    }
                    else if constexpr (std::is_same_v<Command, SeekCommand>)
                    {
                        if (!valid(command.source))
                            return CommandApplyStatus::dropped_stale_target;
                        auto& source = sources[command.source.index];
                        source.cursor_seconds = source.looping
                            ? wrap_cursor(source.clip, command.seconds)
                            : clamp_cursor(source.clip, command.seconds);
                    }
                    else if constexpr (
                        std::is_same_v<Command, SetSourceGainCommand>)
                    {
                        if (!valid(command.source))
                            return CommandApplyStatus::dropped_stale_target;
                        sources[command.source.index].gain = command.gain;
                    }
                    else if constexpr (
                        std::is_same_v<Command, SetSourcePitchCommand>)
                    {
                        if (!valid(command.source))
                            return CommandApplyStatus::dropped_stale_target;
                        sources[command.source.index].pitch = command.pitch;
                    }
                    else if constexpr (std::is_same_v<Command, RouteSourceCommand>)
                    {
                        if (!valid(command.source) || !valid(command.bus))
                            return CommandApplyStatus::dropped_stale_target;
                        sources[command.source.index].bus = command.bus;
                    }
                    else if constexpr (
                        std::is_same_v<Command, SetSpatialStateCommand>)
                    {
                        if (!valid(command.source))
                            return CommandApplyStatus::dropped_stale_target;
                        sources[command.source.index].spatial = command.spatial;
                    }
                    else if constexpr (
                        std::is_same_v<Command, SetPlaybackDirectionCommand>)
                    {
                        if (!valid(command.source))
                            return CommandApplyStatus::dropped_stale_target;
                        sources[command.source.index].direction = command.direction;
                    }
                    else if constexpr (
                        std::is_same_v<Command, SetTemporalPolicyCommand>)
                    {
                        if (!valid(command.source))
                            return CommandApplyStatus::dropped_stale_target;
                        sources[command.source.index].temporal = command.policy;
                    }
                    else if constexpr (std::is_same_v<Command, SetBusGainCommand>)
                    {
                        if (!valid(command.bus))
                            return CommandApplyStatus::dropped_stale_target;
                        buses[command.bus.index].gain = command.gain;
                    }
                    else if constexpr (std::is_same_v<Command, SetBusMuteCommand>)
                    {
                        if (!valid(command.bus))
                            return CommandApplyStatus::dropped_stale_target;
                        buses[command.bus.index].muted = command.muted;
                    }
                    else if constexpr (
                        std::is_same_v<Command, SetListenerStateCommand>)
                    {
                        listener = command.listener;
                    }

                    return CommandApplyStatus::applied;
                },
                pending_command.payload);
        }

        void advance_source(SourceSlot& source, double duration_seconds) noexcept
        {
            if (source.state != PlaybackState::playing)
                return;

            const double signed_duration =
                duration_seconds
                * static_cast<double>(source.pitch)
                * static_cast<double>(
                    static_cast<std::int8_t>(source.direction));
            const double next = source.cursor_seconds + signed_duration;

            if (source.looping)
            {
                source.cursor_seconds = wrap_cursor(source.clip, next);
                return;
            }

            source.cursor_seconds = clamp_cursor(source.clip, next);
            if ((source.direction == PlaybackDirection::forward
                    && next >= source.clip.duration_seconds)
                || (source.direction == PlaybackDirection::reverse
                    && next <= 0.0))
            {
                source.state = PlaybackState::stopped;
            }
        }

        AudioManagerLimits limits{};
        std::vector<SourceSlot> sources{};
        std::vector<BusSlot> buses{};
        std::vector<std::uint32_t> free_sources{};
        std::vector<std::uint32_t> free_buses{};
        std::vector<PendingCommand> pending{};
        ListenerState listener{};
        BusHandle master{};
        std::optional<std::uint64_t> last_compiled_frame{};
        std::uint64_t next_sequence{ 1 };
        std::size_t registered_source_count{};
        std::size_t registered_bus_count{};
        std::uint64_t accepted_command_count{};
        std::uint64_t rejected_command_count{};
        std::uint64_t stale_command_count{};
        std::uint64_t compiled_plan_count{};
        std::uint64_t rejected_plan_count{};
        mutable std::mutex mutex{};
    };

    AudioManager::AudioManager(AudioManagerLimits limits)
        : implementation_{ std::make_unique<Implementation>(limits) }
    {
    }

    AudioManager::~AudioManager() = default;
    AudioManager::AudioManager(AudioManager&&) noexcept = default;
    AudioManager& AudioManager::operator=(AudioManager&&) noexcept = default;

    const AudioManagerLimits& AudioManager::limits() const noexcept
    {
        return implementation_->limits;
    }

    BusHandle AudioManager::master_bus() const noexcept
    {
        return implementation_->master;
    }

    PhysicalOutputState AudioManager::physical_output_state() const noexcept
    {
        return PhysicalOutputState::unavailable_no_backend;
    }

    HandleResult<BusHandle> AudioManager::create_bus(
        const BusDescriptor& descriptor)
    {
        std::scoped_lock lock{ implementation_->mutex };

        const BusHandle parent = descriptor.parent.valid()
            ? descriptor.parent
            : implementation_->master;
        if (!descriptor.id.valid()
            || !implementation_->valid(parent)
            || !valid_gain(descriptor.gain))
        {
            return {
                .status = RegistryStatus::invalid_descriptor,
                .handle = null_bus
            };
        }

        for (std::uint32_t index = 1; index < implementation_->buses.size(); ++index)
        {
            const auto& existing = implementation_->buses[index];
            if (existing.alive && existing.id == descriptor.id)
            {
                return {
                    .status = RegistryStatus::invalid_descriptor,
                    .handle = null_bus
                };
            }
        }

        std::uint32_t index{};
        if (!implementation_->free_buses.empty())
        {
            index = implementation_->free_buses.back();
            implementation_->free_buses.pop_back();
        }
        else
        {
            if (implementation_->registered_bus_count
                >= implementation_->limits.maximum_buses)
            {
                return {
                    .status = RegistryStatus::capacity_reached,
                    .handle = null_bus
                };
            }
            index = static_cast<std::uint32_t>(implementation_->buses.size());
            implementation_->buses.emplace_back();
        }

        auto& slot = implementation_->buses[index];
        slot.alive = true;
        slot.id = descriptor.id;
        slot.parent = parent;
        slot.gain = descriptor.gain;
        slot.muted = descriptor.muted;
        ++implementation_->registered_bus_count;
        return {
            .status = RegistryStatus::success,
            .handle = implementation_->bus_handle(index)
        };
    }

    RegistryStatus AudioManager::destroy_bus(BusHandle bus)
    {
        std::scoped_lock lock{ implementation_->mutex };
        if (!implementation_->valid(bus))
            return RegistryStatus::stale_handle;
        if (bus == implementation_->master)
            return RegistryStatus::reserved_resource;

        for (const auto& source : implementation_->sources)
        {
            if (source.alive && source.bus == bus)
                return RegistryStatus::resource_in_use;
        }
        for (const auto& child : implementation_->buses)
        {
            if (child.alive && child.parent == bus)
                return RegistryStatus::resource_in_use;
        }

        auto& slot = implementation_->buses[bus.index];
        slot.alive = false;
        slot.id = {};
        slot.parent = {};
        slot.gain = 1.0f;
        slot.muted = false;
        slot.generation = next_generation(slot.generation);
        implementation_->free_buses.push_back(bus.index);
        --implementation_->registered_bus_count;
        return RegistryStatus::success;
    }

    HandleResult<SourceHandle> AudioManager::create_source(
        const SourceDescriptor& descriptor)
    {
        std::scoped_lock lock{ implementation_->mutex };
        const BusHandle bus = descriptor.bus.valid()
            ? descriptor.bus
            : implementation_->master;

        if (!valid_clip(descriptor.clip)
            || !implementation_->valid(bus)
            || !valid_gain(descriptor.gain)
            || !valid_pitch(descriptor.pitch)
            || !valid_spatial_state(descriptor.spatial)
            || !valid_temporal_policy(descriptor.clip, descriptor.temporal))
        {
            return {
                .status = RegistryStatus::invalid_descriptor,
                .handle = null_source
            };
        }

        std::uint32_t index{};
        if (!implementation_->free_sources.empty())
        {
            index = implementation_->free_sources.back();
            implementation_->free_sources.pop_back();
        }
        else
        {
            if (implementation_->registered_source_count
                >= implementation_->limits.maximum_sources)
            {
                return {
                    .status = RegistryStatus::capacity_reached,
                    .handle = null_source
                };
            }
            index = static_cast<std::uint32_t>(implementation_->sources.size());
            implementation_->sources.emplace_back();
        }

        auto& slot = implementation_->sources[index];
        slot.alive = true;
        slot.clip = descriptor.clip;
        slot.bus = bus;
        slot.spatial = descriptor.spatial;
        slot.temporal = descriptor.temporal;
        slot.state = PlaybackState::stopped;
        slot.direction = PlaybackDirection::forward;
        slot.cursor_seconds = 0.0;
        slot.gain = descriptor.gain;
        slot.pitch = descriptor.pitch;
        slot.looping = descriptor.looping;
        ++implementation_->registered_source_count;
        return {
            .status = RegistryStatus::success,
            .handle = implementation_->source_handle(index)
        };
    }

    RegistryStatus AudioManager::destroy_source(SourceHandle source)
    {
        std::scoped_lock lock{ implementation_->mutex };
        if (!implementation_->valid(source))
            return RegistryStatus::stale_handle;

        auto& slot = implementation_->sources[source.index];
        slot.alive = false;
        slot.clip = {};
        slot.bus = {};
        slot.spatial = {};
        slot.temporal = {};
        slot.state = PlaybackState::stopped;
        slot.direction = PlaybackDirection::forward;
        slot.cursor_seconds = 0.0;
        slot.gain = 1.0f;
        slot.pitch = 1.0f;
        slot.looping = false;
        slot.generation = next_generation(slot.generation);
        implementation_->free_sources.push_back(source.index);
        --implementation_->registered_source_count;
        return RegistryStatus::success;
    }

    bool AudioManager::valid(BusHandle bus) const noexcept
    {
        std::scoped_lock lock{ implementation_->mutex };
        return implementation_->valid(bus);
    }

    bool AudioManager::valid(SourceHandle source) const noexcept
    {
        std::scoped_lock lock{ implementation_->mutex };
        return implementation_->valid(source);
    }

    std::optional<BusSnapshot> AudioManager::bus_snapshot(BusHandle bus) const
    {
        std::scoped_lock lock{ implementation_->mutex };
        if (!implementation_->valid(bus))
            return std::nullopt;

        const auto& slot = implementation_->buses[bus.index];
        const auto mixed = implementation_->effective_bus_state(bus);
        return BusSnapshot{
            .handle = bus,
            .id = slot.id,
            .parent = slot.parent,
            .gain = slot.gain,
            .effective_gain = mixed.gain,
            .muted = slot.muted,
            .effectively_muted = mixed.muted
        };
    }

    std::optional<SourceSnapshot> AudioManager::source_snapshot(
        SourceHandle source) const
    {
        std::scoped_lock lock{ implementation_->mutex };
        if (!implementation_->valid(source))
            return std::nullopt;

        const auto& slot = implementation_->sources[source.index];
        return SourceSnapshot{
            .handle = source,
            .clip = slot.clip,
            .bus = slot.bus,
            .spatial = slot.spatial,
            .temporal = slot.temporal,
            .state = slot.state,
            .direction = slot.direction,
            .cursor_seconds = slot.cursor_seconds,
            .gain = slot.gain,
            .pitch = slot.pitch,
            .looping = slot.looping
        };
    }

    CommandReceipt AudioManager::submit(const AudioCommandRequest& request)
    {
        std::scoped_lock lock{ implementation_->mutex };

        if (implementation_->pending.size()
            >= implementation_->limits.maximum_pending_commands)
        {
            ++implementation_->rejected_command_count;
            return {
                .status = ScheduleStatus::rejected,
                .reason = ScheduleRejectReason::queue_capacity_reached,
                .sequence = 0,
                .target_frame = request.target_frame
            };
        }

        const auto reason = implementation_->validate_command(request);
        if (reason != ScheduleRejectReason::none)
        {
            ++implementation_->rejected_command_count;
            return {
                .status = ScheduleStatus::rejected,
                .reason = reason,
                .sequence = 0,
                .target_frame = request.target_frame
            };
        }

        const std::uint64_t sequence = implementation_->next_sequence++;
        implementation_->pending.push_back(Implementation::PendingCommand{
            .sequence = sequence,
            .target_frame = request.target_frame,
            .order_group = request.order_group,
            .payload = request.payload
        });
        ++implementation_->accepted_command_count;
        return {
            .status = ScheduleStatus::accepted_logical,
            .reason = ScheduleRejectReason::none,
            .sequence = sequence,
            .target_frame = request.target_frame
        };
    }

    std::vector<CommandReceipt> AudioManager::submit_batch(
        std::span<const AudioCommandRequest> requests)
    {
        std::vector<CommandReceipt> receipts;
        receipts.reserve(requests.size());
        for (const auto& request : requests)
            receipts.push_back(submit(request));
        return receipts;
    }

    FrameMixPlan AudioManager::prepare_frame(const FrameRequest& request)
    {
        std::scoped_lock lock{ implementation_->mutex };

        FrameMixPlan plan{
            .status = FramePlanStatus::ready_logical_only,
            .physical_output = PhysicalOutputState::unavailable_no_backend,
            .frame_index = request.frame_index,
            .timeline_seconds = request.timeline_seconds,
            .duration_seconds = request.duration_seconds,
            .listener = implementation_->listener
        };

        if (!finite(request.timeline_seconds)
            || !finite(request.duration_seconds)
            || request.timeline_seconds < 0.0
            || request.duration_seconds <= 0.0
            || request.duration_seconds > 10.0)
        {
            plan.status = FramePlanStatus::rejected_invalid_timing;
            ++implementation_->rejected_plan_count;
            return plan;
        }

        if (implementation_->last_compiled_frame
            && request.frame_index <= *implementation_->last_compiled_frame)
        {
            plan.status = FramePlanStatus::rejected_non_monotonic_frame;
            ++implementation_->rejected_plan_count;
            return plan;
        }

        std::stable_sort(
            implementation_->pending.begin(),
            implementation_->pending.end(),
            [](const Implementation::PendingCommand& lhs,
               const Implementation::PendingCommand& rhs)
            {
                if (lhs.target_frame != rhs.target_frame)
                    return lhs.target_frame < rhs.target_frame;
                if (lhs.order_group != rhs.order_group)
                    return lhs.order_group < rhs.order_group;
                return lhs.sequence < rhs.sequence;
            });

        std::size_t due_count{};
        while (due_count < implementation_->pending.size()
            && implementation_->pending[due_count].target_frame
                <= request.frame_index)
        {
            ++due_count;
        }

        plan.applied_commands.reserve(due_count);
        for (std::size_t index = 0; index < due_count; ++index)
        {
            const auto& pending = implementation_->pending[index];
            const auto apply_status = implementation_->apply_command(pending);
            plan.applied_commands.push_back(AppliedCommand{
                .sequence = pending.sequence,
                .target_frame = pending.target_frame,
                .order_group = pending.order_group,
                .kind = command_kind(pending.payload),
                .status = apply_status
            });

            ++plan.metrics.commands_considered;
            if (apply_status == CommandApplyStatus::applied)
                ++plan.metrics.commands_applied;
            else if (apply_status == CommandApplyStatus::ignored_for_current_state)
                ++plan.metrics.commands_ignored;
            else
            {
                ++plan.metrics.stale_commands_dropped;
                ++implementation_->stale_command_count;
            }
        }
        implementation_->pending.erase(
            implementation_->pending.begin(),
            implementation_->pending.begin()
                + static_cast<std::ptrdiff_t>(due_count));

        plan.listener = implementation_->listener;
        plan.sources.reserve((std::min)(
            implementation_->registered_source_count,
            static_cast<std::size_t>(
                implementation_->limits.maximum_sources_per_plan)));

        for (std::uint32_t index = 1;
             index < implementation_->sources.size();
             ++index)
        {
            auto& source = implementation_->sources[index];
            if (!source.alive)
                continue;

            ++plan.metrics.registered_sources;
            if (source.state != PlaybackState::playing)
                continue;
            ++plan.metrics.active_sources;

            const double cursor_begin = source.cursor_seconds;
            implementation_->advance_source(source, request.duration_seconds);

            if (plan.sources.size()
                >= implementation_->limits.maximum_sources_per_plan)
            {
                plan.metrics.plan_capacity_limited = true;
                continue;
            }

            const auto bus_state =
                implementation_->effective_bus_state(source.bus);
            const float effective_gain =
                bus_state.muted ? 0.0f : source.gain * bus_state.gain;
            const bool logically_audible = effective_gain > 0.0f;

            plan.sources.push_back(SourceMixPlan{
                .source = implementation_->source_handle(index),
                .clip = source.clip.id,
                .bus = source.bus,
                .direction = source.direction,
                .spatial = source.spatial,
                .cursor_begin_seconds = cursor_begin,
                .cursor_end_seconds = source.cursor_seconds,
                .source_gain = source.gain,
                .effective_bus_gain = bus_state.gain,
                .effective_gain = effective_gain,
                .pitch = source.pitch,
                .logically_audible = logically_audible,
                .looping = source.looping
            });

            if (logically_audible)
                ++plan.metrics.logically_audible_sources;
            else
                ++plan.metrics.muted_sources;
        }

        plan.metrics.planned_sources = plan.sources.size();
        plan.metrics.registered_buses = implementation_->registered_bus_count;
        plan.metrics.pending_commands = implementation_->pending.size();
        plan.metrics.physical_frames_written = 0;
        implementation_->last_compiled_frame = request.frame_index;
        ++implementation_->compiled_plan_count;
        return plan;
    }

    AudioManagerMetrics AudioManager::metrics() const noexcept
    {
        std::scoped_lock lock{ implementation_->mutex };
        return AudioManagerMetrics{
            .registered_sources = implementation_->registered_source_count,
            .registered_buses = implementation_->registered_bus_count,
            .pending_commands = implementation_->pending.size(),
            .commands_accepted = implementation_->accepted_command_count,
            .commands_rejected = implementation_->rejected_command_count,
            .stale_commands_dropped = implementation_->stale_command_count,
            .plans_compiled = implementation_->compiled_plan_count,
            .non_monotonic_plans_rejected = implementation_->rejected_plan_count,
            .physical_output = PhysicalOutputState::unavailable_no_backend
        };
    }

    AudioCommandKind command_kind(
        const AudioCommandPayload& payload) noexcept
    {
        return std::visit(
            [](const auto& command) noexcept
            {
                using Command = std::decay_t<decltype(command)>;
                if constexpr (std::is_same_v<Command, PlayCommand>)
                    return AudioCommandKind::play;
                else if constexpr (std::is_same_v<Command, PauseCommand>)
                    return AudioCommandKind::pause;
                else if constexpr (std::is_same_v<Command, ResumeCommand>)
                    return AudioCommandKind::resume;
                else if constexpr (std::is_same_v<Command, StopCommand>)
                    return AudioCommandKind::stop;
                else if constexpr (std::is_same_v<Command, SeekCommand>)
                    return AudioCommandKind::seek;
                else if constexpr (std::is_same_v<Command, SetSourceGainCommand>)
                    return AudioCommandKind::set_source_gain;
                else if constexpr (std::is_same_v<Command, SetSourcePitchCommand>)
                    return AudioCommandKind::set_source_pitch;
                else if constexpr (std::is_same_v<Command, RouteSourceCommand>)
                    return AudioCommandKind::route_source;
                else if constexpr (std::is_same_v<Command, SetSpatialStateCommand>)
                    return AudioCommandKind::set_spatial_state;
                else if constexpr (
                    std::is_same_v<Command, SetPlaybackDirectionCommand>)
                    return AudioCommandKind::set_playback_direction;
                else if constexpr (
                    std::is_same_v<Command, SetTemporalPolicyCommand>)
                    return AudioCommandKind::set_temporal_policy;
                else if constexpr (std::is_same_v<Command, SetBusGainCommand>)
                    return AudioCommandKind::set_bus_gain;
                else if constexpr (std::is_same_v<Command, SetBusMuteCommand>)
                    return AudioCommandKind::set_bus_mute;
                else
                    return AudioCommandKind::set_listener_state;
            },
            payload);
    }

    AudioContractReport run_audio_manager_contract_tests()
    {
        AudioContractReport report{};
        const auto pass = [&report]()
        {
            ++report.checks_completed;
        };
        const auto fail = [&report](AudioContractFailure failure)
        {
            report.passed = false;
            report.failure = failure;
            return report;
        };

        AudioManager manager{ AudioManagerLimits{
            .maximum_sources = 1,
            .maximum_buses = 2,
            .maximum_pending_commands = 8,
            .maximum_sources_per_plan = 1
        } };

        if (!manager.master_bus().valid()
            || !manager.valid(manager.master_bus()))
        {
            return fail(AudioContractFailure::master_bus_missing);
        }
        pass();

        const SourceDescriptor descriptor{
            .clip = ClipResource{
                .id = ClipId{ .domain = 9, .value = 42 },
                .duration_seconds = 4.0,
                .nominal_sample_rate = 48'000,
                .channel_count = 2,
                .seekable = true,
                .reverse_decodable = false
            },
            .bus = manager.master_bus(),
            .spatial = SpatialState{},
            .temporal = TemporalPlaybackPolicy{
                .seek = SeekPolicy::exact_if_supported,
                .reverse = ReversePlaybackPolicy::disabled,
                .preserve_pitch = true
            },
            .gain = 1.0f,
            .pitch = 1.0f,
            .looping = false
        };

        const auto first = manager.create_source(descriptor);
        if (!first.succeeded())
            return fail(AudioContractFailure::source_creation_failed);
        pass();

        if (manager.create_source(descriptor).status
            != RegistryStatus::capacity_reached)
        {
            return fail(AudioContractFailure::bounds_not_enforced);
        }
        pass();

        const auto reverse_receipt = manager.submit(AudioCommandRequest{
            .target_frame = 1,
            .order_group = 0,
            .payload = SetPlaybackDirectionCommand{
                .source = first.handle,
                .direction = PlaybackDirection::reverse
            }
        });
        if (reverse_receipt.reason
            != ScheduleRejectReason::invalid_temporal_policy)
        {
            return fail(AudioContractFailure::reverse_policy_not_enforced);
        }
        pass();

        if (manager.destroy_source(first.handle) != RegistryStatus::success
            || manager.valid(first.handle)
            || manager.submit(AudioCommandRequest{
                .target_frame = 1,
                .order_group = 0,
                .payload = PlayCommand{
                    .source = first.handle,
                    .start_seconds = 0.0
                }
            }).reason != ScheduleRejectReason::stale_source_handle)
        {
            return fail(AudioContractFailure::stale_source_accepted);
        }
        pass();

        const auto second = manager.create_source(descriptor);
        if (!second.succeeded()
            || second.handle.index != first.handle.index
            || second.handle.generation == first.handle.generation)
        {
            return fail(AudioContractFailure::generation_not_advanced);
        }
        pass();

        const auto play = manager.submit(AudioCommandRequest{
            .target_frame = 2,
            .order_group = 20,
            .payload = PlayCommand{
                .source = second.handle,
                .start_seconds = 1.0
            }
        });
        const auto stop = manager.submit(AudioCommandRequest{
            .target_frame = 2,
            .order_group = 10,
            .payload = StopCommand{
                .source = second.handle,
                .reset_cursor = true
            }
        });
        if (!play.accepted() || !stop.accepted())
            return fail(AudioContractFailure::logical_plan_missing);

        const auto plan = manager.prepare_frame(FrameRequest{
            .frame_index = 2,
            .timeline_seconds = 2.0 / 60.0,
            .duration_seconds = 1.0 / 60.0
        });
        if (!plan.logical_schedule_ready() || plan.sources.size() != 1)
            return fail(AudioContractFailure::logical_plan_missing);
        pass();

        if (plan.applied_commands.size() != 2
            || plan.applied_commands[0].kind != AudioCommandKind::stop
            || plan.applied_commands[1].kind != AudioCommandKind::play
            || plan.applied_commands[0].order_group
                >= plan.applied_commands[1].order_group)
        {
            return fail(AudioContractFailure::command_order_not_deterministic);
        }
        pass();

        if (plan.physical_output_available()
            || plan.physical_output
                != PhysicalOutputState::unavailable_no_backend
            || plan.metrics.physical_frames_written != 0)
        {
            return fail(AudioContractFailure::physical_output_claimed);
        }
        pass();

        AudioManager seekManager{};
        SourceDescriptor unseekableDescriptor = descriptor;
        unseekableDescriptor.bus = seekManager.master_bus();
        unseekableDescriptor.clip.seekable = false;
        unseekableDescriptor.temporal.seek = SeekPolicy::reject_if_unseekable;
        const auto unseekableSource = seekManager.create_source(unseekableDescriptor);
        if (!unseekableSource.succeeded()
            || seekManager.submit(AudioCommandRequest{
                .target_frame = 1,
                .payload = SeekCommand{
                    .source = unseekableSource.handle,
                    .seconds = 1.0
                }
            }).reason != ScheduleRejectReason::invalid_temporal_policy)
        {
            return fail(AudioContractFailure::seek_policy_not_enforced);
        }
        pass();

        AudioManager virtualManager{AudioManagerLimits{
            .maximum_sources = 2,
            .maximum_buses = 1,
            .maximum_pending_commands = 4,
            .maximum_sources_per_plan = 1
        }};
        SourceDescriptor virtualDescriptor = descriptor;
        virtualDescriptor.bus = virtualManager.master_bus();
        const auto virtualFirst = virtualManager.create_source(virtualDescriptor);
        virtualDescriptor.clip.id.value = 43;
        const auto virtualSecond = virtualManager.create_source(virtualDescriptor);
        if (!virtualFirst.succeeded() || !virtualSecond.succeeded()
            || !virtualManager.submit(AudioCommandRequest{
                .target_frame = 1,
                .order_group = 0,
                .payload = PlayCommand{.source = virtualFirst.handle}
            }).accepted()
            || !virtualManager.submit(AudioCommandRequest{
                .target_frame = 1,
                .order_group = 1,
                .payload = PlayCommand{.source = virtualSecond.handle}
            }).accepted())
        {
            return fail(AudioContractFailure::virtual_source_cursor_frozen);
        }
        const auto virtualPlan = virtualManager.prepare_frame({
            .frame_index = 1,
            .timeline_seconds = 0.5,
            .duration_seconds = 0.5
        });
        const auto virtualSecondState = virtualManager.source_snapshot(virtualSecond.handle);
        if (!virtualPlan.metrics.plan_capacity_limited
            || virtualPlan.sources.size() != 1
            || !virtualSecondState.has_value()
            || virtualSecondState->cursor_seconds <= 0.0)
        {
            return fail(AudioContractFailure::virtual_source_cursor_frozen);
        }
        pass();

        report.passed = true;
        report.failure = AudioContractFailure::none;
        return report;
    }
}
