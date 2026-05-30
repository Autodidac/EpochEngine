// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

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

    struct StreamingSaveProfileDescriptor
    {
        StreamingSaveProfile profile = StreamingSaveProfile::ManualReview;
        std::string_view id{};
        std::string_view label{};
        std::string_view summary{};
        SaveStreamMode mode = SaveStreamMode::Manual;
        bool enabled = false;
        double interval_seconds = 15.0;
        std::uint64_t frame_interval = 120;
        std::uint32_t max_snapshots = 32;
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

    struct StreamingCheckpointWritePlan
    {
        bool valid = false;
        std::string root_path{};
        std::string snapshot_path{};
        std::string scene_payload_path{};
        std::string manifest_path{};
        std::string manifest_line{};
        std::string message{};
    };

    struct StreamingCheckpointRestorePlan
    {
        bool valid = false;
        std::string root_path{};
        std::string snapshot_path{};
        std::string scene_payload_path{};
        std::string manifest_path{};
        std::string checkpoint_label{};
        std::string message{};
    };

    struct StreamingCheckpointWriteApproval
    {
        bool approved = false;
        std::string approved_by{};
        std::string reason{};
    };

    struct StreamingCheckpointWriteResult
    {
        bool succeeded = false;
        bool blocked = false;
        bool wrote_snapshot = false;
        bool wrote_scene_payload = false;
        bool wrote_manifest = false;
        std::string message{};
    };

    struct StreamingCheckpointRetentionPlan
    {
        bool valid = false;
        std::uint32_t max_snapshots = 0;
        std::size_t source_count = 0;
        std::size_t retained_count = 0;
        std::vector<std::string> prune_labels{};
        std::vector<std::string> prune_snapshot_paths{};
        std::string message{};
    };

    struct StreamingSaveProfileChangePlan
    {
        bool valid = false;
        std::string from_profile_id{};
        std::string to_profile_id{};
        std::string to_label{};
        SaveStreamMode mode = SaveStreamMode::Manual;
        bool enabled = false;
        double interval_seconds = 0.0;
        std::uint64_t frame_interval = 0;
        std::uint32_t max_snapshots = 0;
        bool include_scene = true;
        bool include_timeline = true;
        bool include_packages = false;
        std::string message{};
    };

    struct StreamingSaveCadencePlan
    {
        bool enabled = false;
        bool capture_due = false;
        SaveStreamMode mode = SaveStreamMode::Manual;
        std::uint64_t current_frame = 0;
        std::uint64_t next_frame = 0;
        std::uint64_t frames_until = 0;
        double current_seconds = 0.0;
        double next_seconds = 0.0;
        double seconds_until = 0.0;
        std::string message{};
    };

    inline constexpr std::array<StreamingSaveProfileDescriptor, 4> kStreamingSaveProfileDescriptors{ {
        {
            .profile = StreamingSaveProfile::ManualReview,
            .id = "manual_review",
            .label = "Manual review",
            .summary = "Manual checkpoint staging for human-reviewed timeline saves.",
            .mode = SaveStreamMode::Manual,
            .enabled = false,
            .interval_seconds = 15.0,
            .frame_interval = 120,
            .max_snapshots = 32,
            .include_scene = true,
            .include_timeline = true,
            .include_packages = false
        },
        {
            .profile = StreamingSaveProfile::EditorInterval15s,
            .id = "editor_interval_15s",
            .label = "Editor 15s stream",
            .summary = "Capture review checkpoints every 15 simulated seconds.",
            .mode = SaveStreamMode::Interval,
            .enabled = true,
            .interval_seconds = 15.0,
            .frame_interval = 120,
            .max_snapshots = 96,
            .include_scene = true,
            .include_timeline = true,
            .include_packages = false
        },
        {
            .profile = StreamingSaveProfile::EditorFrame120,
            .id = "editor_frame_120",
            .label = "Editor 120f stream",
            .summary = "Capture review checkpoints every 120 simulation frames.",
            .mode = SaveStreamMode::FrameInterval,
            .enabled = true,
            .interval_seconds = 15.0,
            .frame_interval = 120,
            .max_snapshots = 120,
            .include_scene = true,
            .include_timeline = true,
            .include_packages = false
        },
        {
            .profile = StreamingSaveProfile::TimelineKeyed,
            .id = "timeline_keyed",
            .label = "Timeline keyed stream",
            .summary = "Capture on explicit timeline keys for deterministic replay gates.",
            .mode = SaveStreamMode::TimelineKey,
            .enabled = true,
            .interval_seconds = 15.0,
            .frame_interval = 120,
            .max_snapshots = 256,
            .include_scene = true,
            .include_timeline = true,
            .include_packages = false
        }
    } };

    [[nodiscard]] inline constexpr const std::array<StreamingSaveProfileDescriptor, 4>& streaming_save_profiles() noexcept
    {
        return kStreamingSaveProfileDescriptors;
    }

    [[nodiscard]] inline constexpr std::size_t streaming_save_profile_count() noexcept
    {
        return kStreamingSaveProfileDescriptors.size();
    }

    [[nodiscard]] inline constexpr const StreamingSaveProfileDescriptor* find_streaming_save_profile(
        StreamingSaveProfile profile) noexcept
    {
        for (const auto& descriptor : kStreamingSaveProfileDescriptors)
        {
            if (descriptor.profile == profile)
                return &descriptor;
        }
        return nullptr;
    }

    [[nodiscard]] inline constexpr const StreamingSaveProfileDescriptor* find_streaming_save_profile(
        std::string_view id) noexcept
    {
        for (const auto& descriptor : kStreamingSaveProfileDescriptors)
        {
            if (descriptor.id == id)
                return &descriptor;
        }
        return nullptr;
    }

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
        if (const auto* descriptor = find_streaming_save_profile(profile))
            return descriptor->label;
        return "Unknown";
    }

    [[nodiscard]] inline std::string_view stream_profile_id(StreamingSaveProfile profile) noexcept
    {
        if (const auto* descriptor = find_streaming_save_profile(profile))
            return descriptor->id;
        return "unknown";
    }

    [[nodiscard]] inline std::string_view stream_profile_summary(StreamingSaveProfile profile) noexcept
    {
        if (const auto* descriptor = find_streaming_save_profile(profile))
            return descriptor->summary;
        return "Unknown streaming-save profile.";
    }

    [[nodiscard]] inline constexpr bool validate_streaming_save_profile_descriptors() noexcept
    {
        bool hasManual = false;
        bool hasInterval = false;
        bool hasFrame = false;
        bool hasKeyed = false;

        for (std::size_t i = 0; i < kStreamingSaveProfileDescriptors.size(); ++i)
        {
            const auto& descriptor = kStreamingSaveProfileDescriptors[i];
            if (descriptor.id.empty() || descriptor.label.empty() || descriptor.summary.empty())
                return false;
            if (descriptor.max_snapshots == 0u)
                return false;
            if (!descriptor.include_scene || !descriptor.include_timeline)
                return false;

            for (std::size_t j = i + 1; j < kStreamingSaveProfileDescriptors.size(); ++j)
            {
                if (descriptor.id == kStreamingSaveProfileDescriptors[j].id
                    || descriptor.profile == kStreamingSaveProfileDescriptors[j].profile)
                {
                    return false;
                }
            }

            switch (descriptor.profile)
            {
            case StreamingSaveProfile::ManualReview:
                hasManual = descriptor.mode == SaveStreamMode::Manual && !descriptor.enabled;
                break;
            case StreamingSaveProfile::EditorInterval15s:
                hasInterval = descriptor.mode == SaveStreamMode::Interval
                    && descriptor.enabled
                    && descriptor.interval_seconds == 15.0;
                break;
            case StreamingSaveProfile::EditorFrame120:
                hasFrame = descriptor.mode == SaveStreamMode::FrameInterval
                    && descriptor.enabled
                    && descriptor.frame_interval == 120u;
                break;
            case StreamingSaveProfile::TimelineKeyed:
                hasKeyed = descriptor.mode == SaveStreamMode::TimelineKey
                    && descriptor.enabled
                    && descriptor.max_snapshots == 256u;
                break;
            default:
                return false;
            }
        }

        return hasManual && hasInterval && hasFrame && hasKeyed;
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
        if (const auto* descriptor = find_streaming_save_profile(profile))
        {
            config.enabled = descriptor->enabled;
            config.mode = descriptor->mode;
            config.interval_seconds = descriptor->interval_seconds;
            config.frame_interval = descriptor->frame_interval;
            config.max_snapshots = descriptor->max_snapshots;
            config.include_scene = descriptor->include_scene;
            config.include_timeline = descriptor->include_timeline;
            config.include_packages = descriptor->include_packages;
        }

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

    [[nodiscard]] inline StreamingSaveCadencePlan make_streaming_save_cadence_plan(
        const StreamingSaveConfig& config,
        const StreamingSaveStatus& status,
        const epoch::core::time::simulation_stats& stats)
    {
        StreamingSaveCadencePlan plan{};
        plan.enabled = config.enabled;
        plan.capture_due = should_capture_checkpoint(config, status, stats);
        plan.mode = config.mode;
        plan.current_frame = stats.frame_index;
        plan.current_seconds = stats.simulated_seconds;

        if (!config.enabled)
        {
            plan.message = "Streaming save is disabled.";
            return plan;
        }

        switch (config.mode)
        {
        case SaveStreamMode::Manual:
            plan.next_frame = stats.frame_index;
            plan.next_seconds = stats.simulated_seconds;
            plan.message = "Manual stream waits for an explicit checkpoint action.";
            break;
        case SaveStreamMode::Interval:
            plan.next_seconds = status.last_simulated_seconds + (std::max)(0.25, config.interval_seconds);
            plan.seconds_until = plan.capture_due ? 0.0 : (std::max)(0.0, plan.next_seconds - stats.simulated_seconds);
            plan.next_frame = stats.frame_index;
            plan.message = plan.capture_due
                ? "Time-interval checkpoint is due now."
                : "Time-interval checkpoint is scheduled.";
            break;
        case SaveStreamMode::FrameInterval:
        {
            const std::uint64_t interval = (std::max)(std::uint64_t{ 1 }, config.frame_interval);
            plan.next_frame = status.last_frame_index + interval;
            plan.frames_until = plan.capture_due || plan.next_frame <= stats.frame_index ? 0u : plan.next_frame - stats.frame_index;
            plan.seconds_until = static_cast<double>(plan.frames_until) * (std::max)(1.0 / 240.0, stats.fixed_dt_seconds);
            plan.next_seconds = stats.simulated_seconds + plan.seconds_until;
            plan.message = plan.capture_due
                ? "Frame-interval checkpoint is due now."
                : "Frame-interval checkpoint is scheduled.";
            break;
        }
        case SaveStreamMode::TimelineKey:
            plan.next_frame = stats.frame_index;
            plan.next_seconds = stats.simulated_seconds;
            plan.message = plan.capture_due
                ? "Timeline-key stream is armed for the next explicit key."
                : "Timeline-key stream is waiting for another explicit key.";
            break;
        default:
            plan.message = "Unknown streaming-save cadence.";
            break;
        }

        return plan;
    }

    [[nodiscard]] inline std::string streaming_save_cadence_summary(const StreamingSaveCadencePlan& plan)
    {
        if (!plan.enabled)
            return plan.message.empty() ? std::string("Streaming save is disabled.") : plan.message;

        if (plan.mode == SaveStreamMode::Manual || plan.mode == SaveStreamMode::TimelineKey)
            return plan.message;

        if (plan.capture_due)
            return std::format("{} | capture due now | frame {} | {:.3f}s", mode_name(plan.mode), plan.current_frame, plan.current_seconds);

        return std::format(
            "{} | next frame {} | {:.3f}s | wait {}f / {:.3f}s",
            mode_name(plan.mode),
            plan.next_frame,
            plan.next_seconds,
            plan.frames_until,
            plan.seconds_until);
    }

    [[nodiscard]] inline std::string checkpoint_label(
        std::string_view profile,
        const epoch::core::time::simulation_stats& stats)
    {
        const std::string safeProfile = profile.empty() ? std::string("editor_timeline") : std::string(profile);
        return std::format("{}_frame_{:012}_t_{:.3f}", safeProfile, stats.frame_index, stats.simulated_seconds);
    }

    [[nodiscard]] inline std::string join_stream_path(std::string_view root, std::string_view leaf)
    {
        std::string result = root.empty() ? std::string("cache/saves/timeline") : std::string(root);
        while (!result.empty() && (result.back() == '/' || result.back() == '\\'))
            result.pop_back();
        if (leaf.empty())
            return result;
        result.push_back('/');
        result += leaf;
        return result;
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

    [[nodiscard]] inline StreamingSaveProfileChangePlan make_streaming_save_profile_change_plan(
        const StreamingSaveConfig& current,
        StreamingSaveProfile target)
    {
        StreamingSaveProfileChangePlan plan{};
        plan.from_profile_id = std::string(stream_profile_id(detect_streaming_save_profile(current)));

        const auto* descriptor = find_streaming_save_profile(target);
        if (descriptor == nullptr)
        {
            plan.message = "Unknown streaming-save profile.";
            return plan;
        }

        StreamingSaveConfig staged = current;
        apply_streaming_save_profile(staged, target);

        plan.valid = true;
        plan.to_profile_id = std::string(descriptor->id);
        plan.to_label = std::string(descriptor->label);
        plan.mode = staged.mode;
        plan.enabled = staged.enabled;
        plan.interval_seconds = staged.interval_seconds;
        plan.frame_interval = staged.frame_interval;
        plan.max_snapshots = staged.max_snapshots;
        plan.include_scene = staged.include_scene;
        plan.include_timeline = staged.include_timeline;
        plan.include_packages = staged.include_packages;
        plan.message = std::format(
            "profile change {} -> {} | {} | {}",
            plan.from_profile_id,
            plan.to_profile_id,
            mode_name(plan.mode),
            describe_retention(staged));
        return plan;
    }

    [[nodiscard]] inline std::string streaming_save_profile_change_summary(
        const StreamingSaveProfileChangePlan& plan)
    {
        if (!plan.valid)
            return plan.message.empty() ? "No streaming-save profile change staged." : plan.message;

        return std::format(
            "{} -> {} | {} | {} | max {}",
            plan.from_profile_id,
            plan.to_profile_id,
            mode_name(plan.mode),
            plan.enabled ? "enabled" : "manual",
            plan.max_snapshots);
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

    [[nodiscard]] inline std::string checkpoint_writer_approval_summary(
        const StreamingCheckpointWriteApproval& approval)
    {
        if (!approval.approved)
            return "Writer gate blocked: human approval is required.";

        return std::format(
            "Writer gate approved by {}: {}",
            approval.approved_by.empty() ? "operator" : approval.approved_by,
            approval.reason.empty() ? "checkpoint persistence" : approval.reason);
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

    [[nodiscard]] inline std::string checkpoint_snapshot_payload(
        const StreamingCheckpointWritePlan& plan,
        const StreamingCheckpointPackage& package)
    {
        return std::format(
            "epoch_checkpoint 1\n"
            "label \"{}\"\n"
            "snapshot \"{}\"\n"
            "scene \"{}\"\n"
            "manifest \"{}\"\n"
            "scene_bytes {}\n"
            "timeline_keys {}\n"
            "hash \"{}\"\n",
            package.record.label,
            plan.snapshot_path,
            plan.scene_payload_path,
            plan.manifest_path,
            package.record.scene_text_bytes,
            package.record.timeline_key_count,
            checkpoint_payload_hash_text(package.scene_text_hash));
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

    [[nodiscard]] inline StreamingCheckpointWritePlan make_checkpoint_write_plan(
        const StreamingSaveConfig& config,
        const StreamingCheckpointPackage& package)
    {
        StreamingCheckpointWritePlan plan{};
        plan.root_path = config.target_root.empty() ? "cache/saves/timeline" : config.target_root;
        plan.snapshot_path = package.record.output_path.empty()
            ? join_stream_path(plan.root_path, package.record.label + ".epochsnap")
            : package.record.output_path;
        plan.scene_payload_path = join_stream_path(plan.root_path, package.record.label + ".epoch");
        plan.manifest_path = join_stream_path(plan.root_path, "manifest.timeline.log");
        plan.manifest_line = package.manifest_line;
        plan.valid = validate_checkpoint_package(package)
            && !package.record.label.empty()
            && !plan.snapshot_path.empty()
            && !plan.scene_payload_path.empty()
            && !plan.manifest_path.empty();
        plan.message = plan.valid
            ? "Checkpoint write plan is ready for the human-approved writer gate."
            : "Checkpoint write plan is blocked by incomplete checkpoint evidence.";
        return plan;
    }

    [[nodiscard]] inline StreamingCheckpointRestorePlan make_checkpoint_restore_plan(
        const StreamingSaveConfig& config,
        const StreamingCheckpointRecord& record)
    {
        StreamingCheckpointRestorePlan plan{};
        plan.root_path = config.target_root.empty() ? "cache/saves/timeline" : config.target_root;
        plan.checkpoint_label = record.label;
        plan.snapshot_path = record.output_path.empty()
            ? join_stream_path(plan.root_path, record.label + ".epochsnap")
            : record.output_path;
        plan.scene_payload_path = join_stream_path(plan.root_path, record.label + ".epoch");
        plan.manifest_path = join_stream_path(plan.root_path, "manifest.timeline.log");
        plan.valid = record.valid
            && !plan.checkpoint_label.empty()
            && !plan.snapshot_path.empty()
            && !plan.scene_payload_path.empty()
            && !plan.manifest_path.empty();
        plan.message = plan.valid
            ? "Checkpoint restore plan is ready for the human-approved replay gate."
            : "Checkpoint restore plan is blocked by missing checkpoint evidence.";
        return plan;
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

    [[nodiscard]] inline std::string checkpoint_write_plan_summary(const StreamingCheckpointWritePlan& plan)
    {
        if (!plan.valid)
            return plan.message.empty() ? std::string("No checkpoint write plan staged.") : plan.message;

        return std::format(
            "write plan | snapshot {} | manifest {} | scene {}",
            plan.snapshot_path,
            plan.manifest_path,
            plan.scene_payload_path);
    }

    [[nodiscard]] inline std::string checkpoint_restore_plan_summary(const StreamingCheckpointRestorePlan& plan)
    {
        if (!plan.valid)
            return plan.message.empty() ? std::string("No checkpoint restore plan staged.") : plan.message;

        return std::format(
            "restore plan | checkpoint {} | snapshot {} | scene {}",
            plan.checkpoint_label,
            plan.snapshot_path,
            plan.scene_payload_path);
    }

    [[nodiscard]] inline StreamingCheckpointRetentionPlan make_checkpoint_retention_plan(
        const StreamingSaveConfig& config,
        std::span<const StreamingCheckpointRecord> records)
    {
        StreamingCheckpointRetentionPlan plan{};
        plan.max_snapshots = (std::max)(1u, config.max_snapshots);
        plan.source_count = records.size();
        plan.valid = config.enabled || !records.empty();

        if (records.size() <= plan.max_snapshots)
        {
            plan.retained_count = records.size();
            plan.message = "Retention plan keeps every staged checkpoint.";
            return plan;
        }

        const std::size_t pruneCount = records.size() - plan.max_snapshots;
        plan.retained_count = plan.max_snapshots;
        plan.prune_labels.reserve(pruneCount);
        plan.prune_snapshot_paths.reserve(pruneCount);
        for (std::size_t index = 0; index < pruneCount; ++index)
        {
            plan.prune_labels.push_back(records[index].label);
            plan.prune_snapshot_paths.push_back(records[index].output_path);
        }

        plan.message = std::format("Retention plan prunes {} old checkpoint{}.", pruneCount, pruneCount == 1u ? "" : "s");
        return plan;
    }

    [[nodiscard]] inline std::string checkpoint_retention_plan_summary(const StreamingCheckpointRetentionPlan& plan)
    {
        if (!plan.valid)
            return "No checkpoint retention plan staged.";

        return std::format(
            "retention plan | retained {}/{} | prune {} | max {}",
            plan.retained_count,
            plan.source_count,
            plan.prune_labels.size(),
            plan.max_snapshots);
    }

    [[nodiscard]] inline bool ensure_parent_directory_for_path(
        std::string_view pathText,
        std::string& errorMessage)
    {
        std::error_code ec{};
        const std::filesystem::path path{ std::string(pathText) };
        const auto parent = path.parent_path();
        if (parent.empty() || std::filesystem::exists(parent, ec))
            return !ec;

        if (std::filesystem::create_directories(parent, ec))
            return true;

        if (!ec && std::filesystem::exists(parent))
            return true;

        errorMessage = std::format("Could not create checkpoint directory: {}", parent.generic_string());
        return false;
    }

    [[nodiscard]] inline StreamingCheckpointWriteResult write_checkpoint_package(
        const StreamingCheckpointWritePlan& plan,
        const StreamingCheckpointPackage& package,
        const StreamingCheckpointWriteApproval& approval)
    {
        StreamingCheckpointWriteResult result{};
        if (!approval.approved)
        {
            result.blocked = true;
            result.message = checkpoint_writer_approval_summary(approval);
            return result;
        }

        if (!plan.valid || !validate_checkpoint_package(package))
        {
            result.message = "Checkpoint writer blocked: write plan or package evidence is invalid.";
            return result;
        }

        if (!ensure_parent_directory_for_path(plan.scene_payload_path, result.message)
            || !ensure_parent_directory_for_path(plan.snapshot_path, result.message)
            || !ensure_parent_directory_for_path(plan.manifest_path, result.message))
        {
            return result;
        }

        {
            std::ofstream sceneOut{ plan.scene_payload_path, std::ios::binary | std::ios::trunc };
            if (!sceneOut)
            {
                result.message = "Checkpoint writer failed to open scene payload.";
                return result;
            }
            sceneOut << package.scene_text;
            result.wrote_scene_payload = true;
        }

        {
            std::ofstream snapshotOut{ plan.snapshot_path, std::ios::binary | std::ios::trunc };
            if (!snapshotOut)
            {
                result.message = "Checkpoint writer failed to open snapshot metadata.";
                return result;
            }
            snapshotOut << checkpoint_snapshot_payload(plan, package);
            result.wrote_snapshot = true;
        }

        {
            std::ofstream manifestOut{ plan.manifest_path, std::ios::binary | std::ios::app };
            if (!manifestOut)
            {
                result.message = "Checkpoint writer failed to open checkpoint manifest.";
                return result;
            }
            manifestOut << plan.manifest_line << '\n';
            result.wrote_manifest = true;
        }

        result.succeeded = result.wrote_scene_payload && result.wrote_snapshot && result.wrote_manifest;
        result.message = result.succeeded
            ? "Checkpoint package written through the approved streaming-save writer gate."
            : "Checkpoint writer did not complete every output.";
        return result;
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
