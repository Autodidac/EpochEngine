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
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

export module scenesnapshot;

export namespace epochengine::scene
{
    using Vec3 = std::array<float, 3>;
    using SceneObjectId = std::uint64_t;

    inline constexpr std::uint32_t kSceneDocumentFormatVersion = 2u;
    inline constexpr SceneObjectId kInvalidSceneObjectId = 0u;

    [[nodiscard]] constexpr SceneObjectId stable_scene_object_id(
        std::string_view scene_id,
        std::string_view object_name) noexcept
    {
        std::uint64_t hash = 14695981039346656037ull;
        const auto append = [&hash](std::string_view value)
        {
            for (const unsigned char character : value)
            {
                hash ^= character;
                hash *= 1099511628211ull;
            }
        };
        append(scene_id);
        hash ^= 0xffu;
        hash *= 1099511628211ull;
        append(object_name);
        return hash == kInvalidSceneObjectId ? 1u : hash;
    }

    struct SceneObjectSnapshot
    {
        SceneObjectId id{ kInvalidSceneObjectId };
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
        std::string project_id{};
        std::string world_name{};
        std::string document_kind{ "game" };
        std::string support_tier{ "baseline" };
        std::uint64_t revision{ 1u };
        SceneObjectId primary_camera{ kInvalidSceneObjectId };
        SceneObjectId primary_spawn{ kInvalidSceneObjectId };
        std::uint64_t captured_frame_index = 0;
        double captured_simulated_seconds = 0.0;
        std::vector<std::string> packages{};
        std::vector<SceneObjectSnapshot> objects{};
        std::vector<SceneTimelineKey> timeline_keys{};
    };

    struct SceneDocumentLimits final
    {
        std::size_t maximum_objects{ 65'536u };
        std::size_t maximum_timeline_keys{ 1'048'576u };
        std::size_t maximum_packages{ 256u };
        std::size_t maximum_string_bytes{ 4'096u };
    };

    enum class SceneDocumentRequirement : std::uint8_t
    {
        authoring,
        runnable
    };

    struct SceneDocumentValidation final
    {
        bool identity_valid{};
        bool counts_bounded{};
        bool strings_bounded{};
        bool object_ids_valid{};
        bool object_ids_unique{};
        bool object_names_unique{};
        bool transforms_valid{};
        bool timeline_valid{};
        bool packages_valid{};
        bool runtime_references_valid{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return identity_valid
                && counts_bounded
                && strings_bounded
                && object_ids_valid
                && object_ids_unique
                && object_names_unique
                && transforms_valid
                && timeline_valid
                && packages_valid
                && runtime_references_valid;
        }
    };

    [[nodiscard]] inline bool finite_scene_vec3(const Vec3& value) noexcept
    {
        return std::isfinite(value[0])
            && std::isfinite(value[1])
            && std::isfinite(value[2]);
    }

    [[nodiscard]] inline SceneObjectSnapshot* find_object_by_id(
        SceneSnapshot& snapshot,
        SceneObjectId id) noexcept
    {
        if (id == kInvalidSceneObjectId)
            return nullptr;
        const auto it = std::find_if(
            snapshot.objects.begin(),
            snapshot.objects.end(),
            [id](const SceneObjectSnapshot& object) { return object.id == id; });
        return it == snapshot.objects.end() ? nullptr : &*it;
    }

    [[nodiscard]] inline const SceneObjectSnapshot* find_object_by_id(
        const SceneSnapshot& snapshot,
        SceneObjectId id) noexcept
    {
        if (id == kInvalidSceneObjectId)
            return nullptr;
        const auto it = std::find_if(
            snapshot.objects.begin(),
            snapshot.objects.end(),
            [id](const SceneObjectSnapshot& object) { return object.id == id; });
        return it == snapshot.objects.end() ? nullptr : &*it;
    }

    inline void normalize_scene_document(SceneSnapshot& snapshot)
    {
        if (snapshot.revision == 0u)
            snapshot.revision = 1u;

        for (SceneObjectSnapshot& object : snapshot.objects)
        {
            if (object.id == kInvalidSceneObjectId)
                object.id = stable_scene_object_id(snapshot.scene_id, object.name);
            if (snapshot.primary_camera == kInvalidSceneObjectId && object.type == "Camera")
                snapshot.primary_camera = object.id;
            if (snapshot.primary_spawn == kInvalidSceneObjectId && object.type == "Spawn")
                snapshot.primary_spawn = object.id;
        }
    }

    [[nodiscard]] inline SceneDocumentValidation validate_scene_document(
        const SceneSnapshot& snapshot,
        SceneDocumentRequirement requirement = SceneDocumentRequirement::authoring,
        const SceneDocumentLimits& limits = {}) noexcept
    {
        SceneDocumentValidation result{};
        result.identity_valid = !snapshot.scene_id.empty()
            && snapshot.revision != 0u
            && (requirement != SceneDocumentRequirement::runnable
                || !snapshot.project_id.empty());
        result.counts_bounded = snapshot.objects.size() <= limits.maximum_objects
            && snapshot.timeline_keys.size() <= limits.maximum_timeline_keys
            && snapshot.packages.size() <= limits.maximum_packages;

        const auto bounded = [&limits](std::string_view value) noexcept
        {
            return value.size() <= limits.maximum_string_bytes;
        };
        result.strings_bounded = bounded(snapshot.scene_id)
            && bounded(snapshot.project_id)
            && bounded(snapshot.world_name)
            && bounded(snapshot.document_kind)
            && bounded(snapshot.support_tier);
        result.object_ids_valid = true;
        result.object_ids_unique = true;
        result.object_names_unique = true;
        result.transforms_valid = true;
        result.timeline_valid = true;
        result.packages_valid = true;

        std::unordered_set<SceneObjectId> object_ids{};
        std::unordered_set<std::string_view> object_names{};
        object_ids.reserve(snapshot.objects.size());
        object_names.reserve(snapshot.objects.size());
        for (const SceneObjectSnapshot& object : snapshot.objects)
        {
            result.strings_bounded = result.strings_bounded
                && bounded(object.name)
                && bounded(object.type)
                && bounded(object.category);
            result.object_ids_valid = result.object_ids_valid
                && object.id != kInvalidSceneObjectId;
            result.object_ids_unique = result.object_ids_unique
                && object_ids.insert(object.id).second;
            result.object_names_unique = result.object_names_unique
                && !object.name.empty()
                && object_names.insert(object.name).second;
            result.transforms_valid = result.transforms_valid
                && finite_scene_vec3(object.position)
                && finite_scene_vec3(object.rotation)
                && finite_scene_vec3(object.scale)
                && object.scale[0] > 0.0f
                && object.scale[1] > 0.0f
                && object.scale[2] > 0.0f;
        }

        std::unordered_set<std::string_view> packages{};
        packages.reserve(snapshot.packages.size());
        for (const std::string& package : snapshot.packages)
        {
            result.packages_valid = result.packages_valid
                && !package.empty()
                && bounded(package)
                && packages.insert(package).second;
        }

        for (const SceneTimelineKey& key : snapshot.timeline_keys)
        {
            result.timeline_valid = result.timeline_valid
                && std::isfinite(key.simulated_seconds)
                && key.simulated_seconds >= 0.0
                && bounded(key.label)
                && bounded(key.event_kind)
                && bounded(key.target_name)
                && bounded(key.payload);
        }

        if (requirement == SceneDocumentRequirement::runnable)
        {
            const SceneObjectSnapshot* camera = find_object_by_id(snapshot, snapshot.primary_camera);
            const SceneObjectSnapshot* spawn = find_object_by_id(snapshot, snapshot.primary_spawn);
            result.runtime_references_valid = camera != nullptr
                && camera->type == "Camera"
                && spawn != nullptr
                && spawn->type == "Spawn"
                && spawn->visible
                && !spawn->editor_only;
        }
        else
        {
            result.runtime_references_valid =
                (snapshot.primary_camera == kInvalidSceneObjectId
                    || find_object_by_id(snapshot, snapshot.primary_camera) != nullptr)
                && (snapshot.primary_spawn == kInvalidSceneObjectId
                    || find_object_by_id(snapshot, snapshot.primary_spawn) != nullptr);
        }
        return result;
    }

    struct SceneRunDescriptor final
    {
        std::string project_id{};
        std::string scene_id{};
        std::uint64_t revision{};
        SceneObjectId camera{ kInvalidSceneObjectId };
        SceneObjectId spawn{ kInvalidSceneObjectId };
        Vec3 spawn_position{};
        Vec3 spawn_rotation{};
    };

    [[nodiscard]] inline std::optional<SceneRunDescriptor> make_scene_run_descriptor(
        const SceneSnapshot& snapshot,
        const SceneDocumentLimits& limits = {})
    {
        if (!validate_scene_document(
                snapshot,
                SceneDocumentRequirement::runnable,
                limits))
        {
            return std::nullopt;
        }

        const SceneObjectSnapshot* spawn = find_object_by_id(snapshot, snapshot.primary_spawn);
        if (spawn == nullptr)
            return std::nullopt;
        return SceneRunDescriptor{
            .project_id = snapshot.project_id,
            .scene_id = snapshot.scene_id,
            .revision = snapshot.revision,
            .camera = snapshot.primary_camera,
            .spawn = snapshot.primary_spawn,
            .spawn_position = spawn->position,
            .spawn_rotation = spawn->rotation
        };
    }

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
