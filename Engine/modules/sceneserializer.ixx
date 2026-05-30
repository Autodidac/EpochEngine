// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

module;

#include <format>
#include <string>
#include <string_view>

export module sceneserializer;

import scenesnapshot;

export namespace epoch::scene
{
    [[nodiscard]] inline std::string escape_snapshot_value(std::string_view value)
    {
        std::string escaped;
        escaped.reserve(value.size());
        for (const char ch : value)
        {
            switch (ch)
            {
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            case '"':
                escaped += "\\\"";
                break;
            default:
                escaped.push_back(ch);
                break;
            }
        }
        return escaped;
    }

    [[nodiscard]] inline std::string vec3_snapshot_text(const Vec3& value)
    {
        return std::format("{:.4f},{:.4f},{:.4f}", value[0], value[1], value[2]);
    }

    [[nodiscard]] inline std::string snapshot_summary(const SceneSnapshot& snapshot)
    {
        return std::format(
            "{} | world {} | frame {} | time {:.3f}s | objects {} | keys {}",
            snapshot.scene_id.empty() ? std::string("(unnamed scene)") : snapshot.scene_id,
            snapshot.world_name.empty() ? std::string("(unnamed world)") : snapshot.world_name,
            snapshot.captured_frame_index,
            snapshot.captured_simulated_seconds,
            snapshot.objects.size(),
            snapshot.timeline_keys.size());
    }

    [[nodiscard]] inline std::string serialize_snapshot_text(const SceneSnapshot& snapshot)
    {
        std::string text;
        text += "epoch_snapshot 1\n";
        text += "scene \"" + escape_snapshot_value(snapshot.scene_id) + "\"\n";
        text += "world \"" + escape_snapshot_value(snapshot.world_name) + "\"\n";
        text += std::format("frame {}\n", snapshot.captured_frame_index);
        text += std::format("time {:.6f}\n", snapshot.captured_simulated_seconds);
        text += std::format("objects {}\n", snapshot.objects.size());

        for (const auto& object : snapshot.objects)
        {
            text += "object \"";
            text += escape_snapshot_value(object.name);
            text += "\" type \"";
            text += escape_snapshot_value(object.type);
            text += "\" category \"";
            text += escape_snapshot_value(object.category);
            text += "\" pos ";
            text += vec3_snapshot_text(object.position);
            text += " rot ";
            text += vec3_snapshot_text(object.rotation);
            text += " scale ";
            text += vec3_snapshot_text(object.scale);
            text += std::format(" visible {} editor_only {}\n", object.visible ? 1 : 0, object.editor_only ? 1 : 0);
        }

        text += std::format("timeline_keys {}\n", snapshot.timeline_keys.size());
        for (const auto& key : snapshot.timeline_keys)
        {
            text += std::format(
                "key {:.6f} {} \"{}\" \"{}\" \"{}\" \"{}\"\n",
                key.simulated_seconds,
                key.frame_index,
                escape_snapshot_value(key.label),
                escape_snapshot_value(key.event_kind),
                escape_snapshot_value(key.target_name),
                escape_snapshot_value(key.payload));
        }

        return text;
    }
}
