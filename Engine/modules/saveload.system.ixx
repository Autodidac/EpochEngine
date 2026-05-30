// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>

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

    enum class StreamingSaveProfile : unsigned char
    {
        ManualReview = 0,
        EditorInterval15s,
        EditorFrame120,
        TimelineKeyed
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

    struct StreamingCheckpointRecord
    {
        bool valid = false;
        std::string label{};
        std::string output_path{};
        std::string stream_profile{};
        SaveStreamMode mode = SaveStreamMode::Manual;
        std::uint64_t frame_index = 0;
        double simulated_seconds = 0.0;
        std::uint32_t retained_snapshot_count = 0;
        bool include_scene = true;
        bool include_timeline = true;
        bool include_packages = false;
        std::size_t scene_text_bytes = 0;
        std::size_t timeline_key_count = 0;
    };

    struct StreamingCheckpointPackage
    {
        bool valid = false;
        StreamingCheckpointRecord record{};
        std::string manifest_line{};
        std::string scene_text{};
        std::uint64_t scene_text_hash = 0;
        std::string message{};
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

    [[nodiscard]] inline std::string_view stream_profile_name(StreamingSaveProfile profile) noexcept
    {
        switch (profile)
        {
        case StreamingSaveProfile::ManualReview:
            return "Manual review";
        case StreamingSaveProfile::EditorInterval15s:
            return "Editor 15s stream";
        case StreamingSaveProfile::EditorFrame120:
            return "Editor 120f stream";
        case StreamingSaveProfile::TimelineKeyed:
            return "Timeline keyed stream";
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

    inline void apply_streaming_save_profile(
        StreamingSaveConfig& config,
        StreamingSaveProfile profile)
    {
        switch (profile)
        {
        case StreamingSaveProfile::ManualReview:
            config.enabled = false;
            config.mode = SaveStreamMode::Manual;
            config.max_snapshots = 32;
            break;
        case StreamingSaveProfile::EditorInterval15s:
            config.enabled = true;
            config.mode = SaveStreamMode::Interval;
            config.interval_seconds = 15.0;
            config.max_snapshots = 96;
            break;
        case StreamingSaveProfile::EditorFrame120:
            config.enabled = true;
            config.mode = SaveStreamMode::FrameInterval;
            config.frame_interval = 120;
            config.max_snapshots = 120;
            break;
        case StreamingSaveProfile::TimelineKeyed:
            config.enabled = true;
            config.mode = SaveStreamMode::TimelineKey;
            config.max_snapshots = 256;
            break;
        default:
            break;
        }

        config.include_scene = true;
        config.include_timeline = true;
        clamp_streaming_save_config(config);
    }

    [[nodiscard]] inline StreamingSaveProfile detect_streaming_save_profile(
        const StreamingSaveConfig& config) noexcept
    {
        if (!config.enabled || config.mode == SaveStreamMode::Manual)
            return StreamingSaveProfile::ManualReview;
        if (config.mode == SaveStreamMode::Interval)
            return StreamingSaveProfile::EditorInterval15s;
        if (config.mode == SaveStreamMode::FrameInterval)
            return StreamingSaveProfile::EditorFrame120;
        if (config.mode == SaveStreamMode::TimelineKey)
            return StreamingSaveProfile::TimelineKeyed;
        return StreamingSaveProfile::ManualReview;
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

    [[nodiscard]] inline StreamingCheckpointRecord make_checkpoint_record(
        const StreamingSaveConfig& config,
        const StreamingSaveStatus& status,
        const epoch::core::time::simulation_stats& stats,
        std::size_t scene_text_bytes,
        std::size_t timeline_key_count)
    {
        return StreamingCheckpointRecord{
            .valid = !status.last_snapshot_label.empty() && !status.last_output_path.empty(),
            .label = status.last_snapshot_label,
            .output_path = status.last_output_path,
            .stream_profile = std::string(stream_profile_name(detect_streaming_save_profile(config))),
            .mode = config.mode,
            .frame_index = stats.frame_index,
            .simulated_seconds = stats.simulated_seconds,
            .retained_snapshot_count = status.staged_snapshot_count,
            .include_scene = config.include_scene,
            .include_timeline = config.include_timeline,
            .include_packages = config.include_packages,
            .scene_text_bytes = scene_text_bytes,
            .timeline_key_count = timeline_key_count
        };
    }

    [[nodiscard]] inline std::string describe_retention(const StreamingSaveConfig& config)
    {
        return std::format(
            "rolling {} checkpoint{} | scene {} | timeline {} | packages {}",
            config.max_snapshots,
            config.max_snapshots == 1u ? "" : "s",
            config.include_scene ? "on" : "off",
            config.include_timeline ? "on" : "off",
            config.include_packages ? "on" : "off");
    }

    [[nodiscard]] inline std::string checkpoint_record_summary(const StreamingCheckpointRecord& record)
    {
        if (!record.valid)
            return "No checkpoint record staged.";

        return std::format(
            "{} | {} | frame {} | {:.3f}s | {} keys | {} bytes",
            record.stream_profile,
            record.label,
            record.frame_index,
            record.simulated_seconds,
            record.timeline_key_count,
            record.scene_text_bytes);
    }

    [[nodiscard]] inline std::uint64_t checkpoint_payload_hash(std::string_view payload) noexcept
    {
        std::uint64_t hash = 14'695'981'039'346'656'037ull;
        for (const char ch : payload)
        {
            hash ^= static_cast<unsigned char>(ch);
            hash *= 1'099'511'628'211ull;
        }
        return hash;
    }

    [[nodiscard]] inline std::string checkpoint_payload_hash_text(std::uint64_t hash)
    {
        return std::format("{:016X}", hash);
    }

    [[nodiscard]] inline std::string checkpoint_manifest_line(const StreamingCheckpointRecord& record)
    {
        if (!record.valid)
            return "checkpoint invalid";

        return std::format(
            "checkpoint \"{}\" path \"{}\" mode \"{}\" frame {} time {:.6f} scene_bytes {} timeline_keys {}",
            record.label,
            record.output_path,
            mode_name(record.mode),
            record.frame_index,
            record.simulated_seconds,
            record.scene_text_bytes,
            record.timeline_key_count);
    }

    [[nodiscard]] inline StreamingCheckpointPackage make_checkpoint_package(
        const StreamingCheckpointRecord& record,
        std::string scene_text)
    {
        StreamingCheckpointPackage package{};
        package.record = record;
        package.scene_text = std::move(scene_text);
        package.scene_text_hash = checkpoint_payload_hash(package.scene_text);
        package.valid = record.valid
            && !package.scene_text.empty()
            && record.scene_text_bytes == package.scene_text.size();
        package.manifest_line = checkpoint_manifest_line(record);
        if (package.valid)
        {
            package.manifest_line += " hash \"";
            package.manifest_line += checkpoint_payload_hash_text(package.scene_text_hash);
            package.manifest_line += "\"";
            package.message = "Checkpoint package staged for deterministic restore.";
        }
        else
        {
            package.message = "Checkpoint package is invalid or incomplete.";
        }
        return package;
    }

    [[nodiscard]] inline bool validate_checkpoint_package(const StreamingCheckpointPackage& package)
    {
        if (!package.valid || !package.record.valid || package.scene_text.empty())
            return false;
        if (package.record.scene_text_bytes != package.scene_text.size())
            return false;
        if (package.scene_text_hash != checkpoint_payload_hash(package.scene_text))
            return false;
        return package.manifest_line.find(checkpoint_payload_hash_text(package.scene_text_hash)) != std::string::npos;
    }

    [[nodiscard]] inline std::string checkpoint_package_summary(const StreamingCheckpointPackage& package)
    {
        if (!validate_checkpoint_package(package))
            return "No valid checkpoint package staged.";

        return std::format(
            "{} | hash {} | {}",
            checkpoint_record_summary(package.record),
            checkpoint_payload_hash_text(package.scene_text_hash),
            package.message);
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
