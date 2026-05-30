// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

module;

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module timeline.system;

import core.time;
import scenesnapshot;

export namespace epoch::timeline
{
    enum class TimelineTrackKind : unsigned char
    {
        Scene = 0,
        Camera,
        Save,
        Package,
        Simulation
    };

    enum class TimelineEventKind : unsigned char
    {
        Checkpoint = 0,
        SceneMutation,
        CameraCut,
        PackageActivation,
        SimulationMarker
    };

    struct TimelineTrack
    {
        std::string id{};
        std::string label{};
        TimelineTrackKind kind = TimelineTrackKind::Scene;
        bool enabled = true;
        bool locked = false;
    };

    struct TimelineEvent
    {
        std::string track_id{};
        TimelineEventKind kind = TimelineEventKind::Checkpoint;
        double simulated_seconds = 0.0;
        std::uint64_t frame_index = 0;
        std::string label{};
        std::string target_name{};
        std::string payload{};
    };

    struct TimelineState
    {
        double playhead_seconds = 0.0;
        std::uint64_t playhead_frame = 0;
        double duration_seconds = 120.0;
        double fixed_dt_seconds = 1.0 / 60.0;
        bool playing = false;
        bool recording = false;
    };

    [[nodiscard]] inline std::string_view track_kind_name(TimelineTrackKind kind) noexcept
    {
        switch (kind)
        {
        case TimelineTrackKind::Scene:
            return "Scene";
        case TimelineTrackKind::Camera:
            return "Camera";
        case TimelineTrackKind::Save:
            return "Save";
        case TimelineTrackKind::Package:
            return "Package";
        case TimelineTrackKind::Simulation:
            return "Simulation";
        default:
            return "Unknown";
        }
    }

    [[nodiscard]] inline std::string_view event_kind_name(TimelineEventKind kind) noexcept
    {
        switch (kind)
        {
        case TimelineEventKind::Checkpoint:
            return "Checkpoint";
        case TimelineEventKind::SceneMutation:
            return "Scene mutation";
        case TimelineEventKind::CameraCut:
            return "Camera cut";
        case TimelineEventKind::PackageActivation:
            return "Package activation";
        case TimelineEventKind::SimulationMarker:
            return "Simulation marker";
        default:
            return "Unknown";
        }
    }

    inline void clamp_state(TimelineState& state) noexcept
    {
        state.duration_seconds = (std::clamp)(state.duration_seconds, 1.0, 86'400.0);
        state.fixed_dt_seconds = (std::clamp)(state.fixed_dt_seconds, 1.0 / 240.0, 1.0 / 15.0);
        state.playhead_seconds = (std::clamp)(state.playhead_seconds, 0.0, state.duration_seconds);
        state.playhead_frame = static_cast<std::uint64_t>(state.playhead_seconds / state.fixed_dt_seconds + 0.5);
    }

    inline void sync_to_simulation(TimelineState& state, const epoch::core::time::simulation_stats& stats) noexcept
    {
        state.fixed_dt_seconds = stats.fixed_dt_seconds;
        if (state.playing)
        {
            state.playhead_seconds = stats.simulated_seconds;
            state.playhead_frame = stats.frame_index;
            clamp_state(state);
        }
    }

    inline void scrub_seconds(TimelineState& state, double delta_seconds) noexcept
    {
        state.playhead_seconds += delta_seconds;
        clamp_state(state);
    }

    [[nodiscard]] inline TimelineEvent make_event_from_stats(
        std::string track_id,
        TimelineEventKind kind,
        const epoch::core::time::simulation_stats& stats,
        std::string label,
        std::string target_name,
        std::string payload = {})
    {
        return TimelineEvent{
            .track_id = std::move(track_id),
            .kind = kind,
            .simulated_seconds = stats.simulated_seconds,
            .frame_index = stats.frame_index,
            .label = std::move(label),
            .target_name = std::move(target_name),
            .payload = std::move(payload)
        };
    }

    inline void sort_events(std::vector<TimelineEvent>& events)
    {
        std::sort(
            events.begin(),
            events.end(),
            [](const TimelineEvent& lhs, const TimelineEvent& rhs)
            {
                if (lhs.simulated_seconds == rhs.simulated_seconds)
                    return lhs.frame_index < rhs.frame_index;
                return lhs.simulated_seconds < rhs.simulated_seconds;
            });
    }

    [[nodiscard]] inline std::size_t enabled_track_count(const std::vector<TimelineTrack>& tracks) noexcept
    {
        return static_cast<std::size_t>(std::count_if(
            tracks.begin(),
            tracks.end(),
            [](const TimelineTrack& track)
            {
                return track.enabled;
            }));
    }

    [[nodiscard]] inline const TimelineEvent* next_event_after(
        const std::vector<TimelineEvent>& events,
        double simulated_seconds) noexcept
    {
        const auto it = std::find_if(
            events.begin(),
            events.end(),
            [&](const TimelineEvent& event)
            {
                return event.simulated_seconds >= simulated_seconds;
            });
        return it == events.end() ? nullptr : &*it;
    }

    [[nodiscard]] inline std::string describe_event(const TimelineEvent& event)
    {
        return std::format(
            "{} @ {:.3f}s / frame {} -> {}",
            event_kind_name(event.kind),
            event.simulated_seconds,
            event.frame_index,
            event.label.empty() ? std::string("(unlabeled)") : event.label);
    }

    [[nodiscard]] inline epoch::scene::SceneTimelineKey to_scene_timeline_key(const TimelineEvent& event)
    {
        return epoch::scene::make_timeline_key(
            event.simulated_seconds,
            event.frame_index,
            event.label,
            std::string(event_kind_name(event.kind)),
            event.target_name,
            event.payload);
    }

    [[nodiscard]] inline std::vector<TimelineTrack> default_editor_tracks()
    {
        return {
            TimelineTrack{ .id = "scene", .label = "Scene", .kind = TimelineTrackKind::Scene },
            TimelineTrack{ .id = "camera", .label = "Camera", .kind = TimelineTrackKind::Camera },
            TimelineTrack{ .id = "save", .label = "Streaming Save", .kind = TimelineTrackKind::Save },
            TimelineTrack{ .id = "simulation", .label = "Simulation", .kind = TimelineTrackKind::Simulation }
        };
    }
}
