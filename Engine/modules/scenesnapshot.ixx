// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

module;

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

export module scenesnapshot;

export namespace epoch::scene
{
    using Vec3 = std::array<float, 3>;

    struct SceneObjectSnapshot
    {
        std::string name{};
        std::string type{};
        std::string category{};
        Vec3 position{ 0.0f, 0.0f, 0.0f };
        Vec3 rotation{ 0.0f, 0.0f, 0.0f };
        Vec3 scale{ 1.0f, 1.0f, 1.0f };
        bool visible = true;
        bool editor_only = false;
    };

    struct SceneTimelineKey
    {
        double simulated_seconds = 0.0;
        std::uint64_t frame_index = 0;
        std::string label{};
        std::string event_kind{};
        std::string target_name{};
        std::string payload{};
    };

    struct SceneSnapshot
    {
        std::string scene_id{};
        std::string world_name{};
        std::uint64_t captured_frame_index = 0;
        double captured_simulated_seconds = 0.0;
        std::vector<SceneObjectSnapshot> objects{};
        std::vector<SceneTimelineKey> timeline_keys{};
    };

    [[nodiscard]] inline const SceneObjectSnapshot* find_object(
        const SceneSnapshot& snapshot,
        std::string_view name) noexcept
    {
        const auto it = std::find_if(
            snapshot.objects.begin(),
            snapshot.objects.end(),
            [&](const SceneObjectSnapshot& object)
            {
                return object.name == name;
            });
        return it == snapshot.objects.end() ? nullptr : &*it;
    }

    [[nodiscard]] inline std::unordered_map<std::string, std::size_t> object_count_by_category(
        const SceneSnapshot& snapshot)
    {
        std::unordered_map<std::string, std::size_t> counts;
        for (const auto& object : snapshot.objects)
            ++counts[object.category.empty() ? std::string("Uncategorized") : object.category];
        return counts;
    }

    [[nodiscard]] inline SceneTimelineKey make_timeline_key(
        double simulated_seconds,
        std::uint64_t frame_index,
        std::string label,
        std::string event_kind,
        std::string target_name,
        std::string payload = {})
    {
        return SceneTimelineKey{
            .simulated_seconds = simulated_seconds,
            .frame_index = frame_index,
            .label = std::move(label),
            .event_kind = std::move(event_kind),
            .target_name = std::move(target_name),
            .payload = std::move(payload)
        };
    }

    inline void sort_timeline_keys(SceneSnapshot& snapshot)
    {
        std::sort(
            snapshot.timeline_keys.begin(),
            snapshot.timeline_keys.end(),
            [](const SceneTimelineKey& lhs, const SceneTimelineKey& rhs)
            {
                if (lhs.simulated_seconds == rhs.simulated_seconds)
                    return lhs.frame_index < rhs.frame_index;
                return lhs.simulated_seconds < rhs.simulated_seconds;
            });
    }
}
