// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

module;

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

export module saveload.system;

import core.time;

export namespace epoch::saveload
{
    enum class SaveStreamMode : unsigned char
    {
        Manual = 0,
        Interval,
        FrameInterval,
        TimelineKey
    };

    struct StreamingSaveConfig
    {
        bool enabled = false;
        SaveStreamMode mode = SaveStreamMode::Manual;
        double interval_seconds = 15.0;
        std::uint64_t frame_interval = 120;
        std::uint32_t max_snapshots = 32;
        std::string profile_name = "editor_timeline";
        std::string target_root = "cache/saves/timeline";
        bool include_scene = true;
        bool include_timeline = true;
        bool include_packages = false;
    };

    struct StreamingSaveStatus
    {
        bool active = false;
        std::uint64_t last_frame_index = 0;
        double last_simulated_seconds = 0.0;
        std::uint32_t staged_snapshot_count = 0;
        std::string last_snapshot_label{};
        std::string last_output_path{};
        std::string message = "Timeline save stream is disabled.";
    };

    [[nodiscard]] inline std::string_view mode_name(SaveStreamMode mode) noexcept
    {
        switch (mode)
        {
        case SaveStreamMode::Manual:
            return "Manual";
        case SaveStreamMode::Interval:
            return "Time interval";
        case SaveStreamMode::FrameInterval:
            return "Frame interval";
        case SaveStreamMode::TimelineKey:
            return "Timeline key";
        default:
            return "Unknown";
        }
    }

    inline void clamp_streaming_save_config(StreamingSaveConfig& config) noexcept
    {
        config.interval_seconds = (std::clamp)(config.interval_seconds, 0.25, 3600.0);
        config.frame_interval = (std::clamp)(config.frame_interval, std::uint64_t{ 1 }, std::uint64_t{ 1'000'000 });
        config.max_snapshots = (std::clamp)(config.max_snapshots, 1u, 4096u);
        if (config.profile_name.empty())
            config.profile_name = "editor_timeline";
        if (config.target_root.empty())
            config.target_root = "cache/saves/timeline";
    }

    [[nodiscard]] inline bool should_capture_checkpoint(
        const StreamingSaveConfig& config,
        const StreamingSaveStatus& status,
        const epoch::core::time::simulation_stats& stats) noexcept
    {
        if (!config.enabled)
            return false;

        switch (config.mode)
        {
        case SaveStreamMode::Manual:
            return false;
        case SaveStreamMode::Interval:
            return stats.simulated_seconds - status.last_simulated_seconds >= config.interval_seconds;
        case SaveStreamMode::FrameInterval:
            return stats.frame_index >= status.last_frame_index
                && stats.frame_index - status.last_frame_index >= config.frame_interval;
        case SaveStreamMode::TimelineKey:
            return status.staged_snapshot_count == 0;
        default:
            return false;
        }
    }

    [[nodiscard]] inline std::string checkpoint_label(
        std::string_view profile,
        const epoch::core::time::simulation_stats& stats)
    {
        const std::string safeProfile = profile.empty() ? std::string("editor_timeline") : std::string(profile);
        return std::format("{}_frame_{:012}_t_{:.3f}", safeProfile, stats.frame_index, stats.simulated_seconds);
    }

    inline void mark_checkpoint_captured(
        StreamingSaveStatus& status,
        const StreamingSaveConfig& config,
        const epoch::core::time::simulation_stats& stats)
    {
        status.active = config.enabled;
        status.last_frame_index = stats.frame_index;
        status.last_simulated_seconds = stats.simulated_seconds;
        status.staged_snapshot_count = (std::min)(
            status.staged_snapshot_count + 1u,
            config.max_snapshots);
        status.last_snapshot_label = checkpoint_label(config.profile_name, stats);
        status.last_output_path = config.target_root + "/" + status.last_snapshot_label + ".epochsnap";
        status.message = "Timeline checkpoint staged for review.";
    }

    [[nodiscard]] inline std::string describe_streaming_save(
        const StreamingSaveConfig& config,
        const StreamingSaveStatus& status)
    {
        return std::format(
            "{} | {} | {} snapshots | target {}",
            config.enabled ? "enabled" : "disabled",
            mode_name(config.mode),
            status.staged_snapshot_count,
            config.target_root);
    }
}
