/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module timeline.system;

import core.time;
import scenesnapshot;

export namespace epochengine::timeline
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

    struct TimelineViewConfig
    {
        double visible_start_seconds = 0.0;
        double visible_duration_seconds = 10.0;
        double pixel_width = 640.0;
    };

    struct TimelineViewMetrics
    {
        double visible_start_seconds = 0.0;
        double visible_end_seconds = 10.0;
        double visible_duration_seconds = 10.0;
        double seconds_per_pixel = 1.0 / 64.0;
        double playhead_x = 0.0;
        std::size_t visible_event_count = 0;
        std::size_t enabled_track_count = 0;
    };

    struct TimelineTrackSummary
    {
        std::string id{};
        std::string label{};
        TimelineTrackKind kind = TimelineTrackKind::Scene;
        std::size_t event_count = 0;
        bool enabled = true;
        bool locked = false;
    };

    struct TimelineLaneLayoutConfig
    {
        double pixel_width = 640.0;
        double header_width = 120.0;
        double lane_height = 28.0;
        double lane_gap = 4.0;
        double top_padding = 0.0;
    };

    struct TimelineLaneGeometry
    {
        std::string track_id{};
        std::string label{};
        TimelineTrackKind kind = TimelineTrackKind::Scene;
        double x = 0.0;
        double y = 0.0;
        double width = 0.0;
        double height = 28.0;
        bool enabled = true;
        bool locked = false;
    };

    struct TimelineEventMarker
    {
        std::string track_id{};
        std::string label{};
        TimelineEventKind kind = TimelineEventKind::Checkpoint;
        double x = 0.0;
        double y = 0.0;
        double radius = 4.0;
        bool visible = false;
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

    inline void sync_to_simulation(TimelineState& state, const epochengine::core::time::simulation_stats& stats) noexcept
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

    inline void clamp_view_config(TimelineViewConfig& view, double timeline_duration_seconds) noexcept
    {
        const double duration = (std::max)(1.0, timeline_duration_seconds);
        view.pixel_width = (std::clamp)(view.pixel_width, 64.0, 65'536.0);
        view.visible_duration_seconds = (std::clamp)(view.visible_duration_seconds, 0.25, duration);
        view.visible_start_seconds = (std::clamp)(
            view.visible_start_seconds,
            0.0,
            (std::max)(0.0, duration - view.visible_duration_seconds));
    }

    inline void clamp_lane_layout_config(TimelineLaneLayoutConfig& layout) noexcept
    {
        layout.pixel_width = (std::clamp)(layout.pixel_width, 128.0, 65'536.0);
        layout.header_width = (std::clamp)(layout.header_width, 0.0, layout.pixel_width * 0.75);
        layout.lane_height = (std::clamp)(layout.lane_height, 12.0, 240.0);
        layout.lane_gap = (std::clamp)(layout.lane_gap, 0.0, 80.0);
        layout.top_padding = (std::clamp)(layout.top_padding, 0.0, 4096.0);
    }

    [[nodiscard]] inline double lane_content_width(TimelineLaneLayoutConfig layout) noexcept
    {
        clamp_lane_layout_config(layout);
        return (std::max)(1.0, layout.pixel_width - layout.header_width);
    }

    [[nodiscard]] inline bool event_is_visible(
        const TimelineEvent& event,
        const TimelineViewConfig& view) noexcept
    {
        return event.simulated_seconds >= view.visible_start_seconds &&
               event.simulated_seconds <= view.visible_start_seconds + view.visible_duration_seconds;
    }

    [[nodiscard]] inline double event_position_x(
        const TimelineEvent& event,
        const TimelineViewConfig& view) noexcept
    {
        const double normalized = (event.simulated_seconds - view.visible_start_seconds) /
            (std::max)(0.000001, view.visible_duration_seconds);
        return (std::clamp)(normalized, 0.0, 1.0) * view.pixel_width;
    }

    [[nodiscard]] inline TimelineEvent make_event_from_stats(
        std::string track_id,
        TimelineEventKind kind,
        const epochengine::core::time::simulation_stats& stats,
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

    [[nodiscard]] inline std::vector<TimelineTrackSummary> summarize_tracks(
        const std::vector<TimelineTrack>& tracks,
        const std::vector<TimelineEvent>& events)
    {
        std::vector<TimelineTrackSummary> summaries;
        summaries.reserve(tracks.size());

        for (const auto& track : tracks)
        {
            const auto event_count = static_cast<std::size_t>(std::count_if(
                events.begin(),
                events.end(),
                [&](const TimelineEvent& event)
                {
                    return event.track_id == track.id;
                }));

            summaries.push_back(TimelineTrackSummary{
                .id = track.id,
                .label = track.label,
                .kind = track.kind,
                .event_count = event_count,
                .enabled = track.enabled,
                .locked = track.locked
            });
        }

        return summaries;
    }

    [[nodiscard]] inline TimelineViewMetrics make_view_metrics(
        TimelineState state,
        const std::vector<TimelineTrack>& tracks,
        const std::vector<TimelineEvent>& events,
        TimelineViewConfig view)
    {
        clamp_state(state);
        clamp_view_config(view, state.duration_seconds);

        TimelineViewMetrics metrics{};
        metrics.visible_start_seconds = view.visible_start_seconds;
        metrics.visible_end_seconds = view.visible_start_seconds + view.visible_duration_seconds;
        metrics.visible_duration_seconds = view.visible_duration_seconds;
        metrics.seconds_per_pixel = view.visible_duration_seconds / view.pixel_width;
        metrics.playhead_x = (std::clamp)(
            (state.playhead_seconds - view.visible_start_seconds) /
                (std::max)(0.000001, view.visible_duration_seconds),
            0.0,
            1.0) * view.pixel_width;
        metrics.enabled_track_count = enabled_track_count(tracks);
        metrics.visible_event_count = static_cast<std::size_t>(std::count_if(
            events.begin(),
            events.end(),
            [&](const TimelineEvent& event)
            {
                return event_is_visible(event, view);
            }));
        return metrics;
    }

    [[nodiscard]] inline std::vector<TimelineLaneGeometry> make_lane_geometry(
        const std::vector<TimelineTrack>& tracks,
        TimelineLaneLayoutConfig layout)
    {
        clamp_lane_layout_config(layout);
        std::vector<TimelineLaneGeometry> lanes;
        lanes.reserve(tracks.size());

        double y = layout.top_padding;
        for (const auto& track : tracks)
        {
            lanes.push_back(TimelineLaneGeometry{
                .track_id = track.id,
                .label = track.label,
                .kind = track.kind,
                .x = layout.header_width,
                .y = y,
                .width = lane_content_width(layout),
                .height = layout.lane_height,
                .enabled = track.enabled,
                .locked = track.locked
            });
            y += layout.lane_height + layout.lane_gap;
        }

        return lanes;
    }

    [[nodiscard]] inline std::vector<TimelineEventMarker> make_event_markers(
        const std::vector<TimelineTrack>& tracks,
        const std::vector<TimelineEvent>& events,
        TimelineViewConfig view,
        TimelineLaneLayoutConfig layout,
        double timeline_duration_seconds)
    {
        clamp_view_config(view, timeline_duration_seconds);
        clamp_lane_layout_config(layout);
        view.pixel_width = lane_content_width(layout);

        const auto lanes = make_lane_geometry(tracks, layout);
        std::vector<TimelineEventMarker> markers;
        markers.reserve(events.size());

        for (const auto& event : events)
        {
            const auto laneIt = std::find_if(
                lanes.begin(),
                lanes.end(),
                [&](const TimelineLaneGeometry& lane)
                {
                    return lane.track_id == event.track_id;
                });
            if (laneIt == lanes.end())
                continue;

            const bool visible = event_is_visible(event, view);
            markers.push_back(TimelineEventMarker{
                .track_id = event.track_id,
                .label = event.label,
                .kind = event.kind,
                .x = layout.header_width + event_position_x(event, view),
                .y = laneIt->y + (laneIt->height * 0.5),
                .radius = visible ? 5.0 : 3.0,
                .visible = visible
            });
        }

        return markers;
    }

    [[nodiscard]] inline std::string describe_lane_layout(
        const std::vector<TimelineLaneGeometry>& lanes,
        const std::vector<TimelineEventMarker>& markers)
    {
        const auto visibleMarkers = static_cast<std::size_t>(std::count_if(
            markers.begin(),
            markers.end(),
            [](const TimelineEventMarker& marker)
            {
                return marker.visible;
            }));
        return std::format(
            "{} lanes | {} markers | {} visible",
            lanes.size(),
            markers.size(),
            visibleMarkers);
    }

    [[nodiscard]] inline std::string describe_view(
        const TimelineState& state,
        const std::vector<TimelineTrack>& tracks,
        const std::vector<TimelineEvent>& events,
        const TimelineViewConfig& view)
    {
        const auto metrics = make_view_metrics(state, tracks, events, view);
        return std::format(
            "timeline view {:.2f}-{:.2f}s | playhead x {:.1f} | events {} | tracks {}",
            metrics.visible_start_seconds,
            metrics.visible_end_seconds,
            metrics.playhead_x,
            metrics.visible_event_count,
            metrics.enabled_track_count);
    }

    [[nodiscard]] inline epochengine::scene::SceneTimelineKey to_scene_timeline_key(const TimelineEvent& event)
    {
        return epochengine::scene::make_timeline_key(
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
