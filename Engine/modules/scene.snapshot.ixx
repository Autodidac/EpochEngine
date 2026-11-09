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

export module scene.snapshot;

export namespace epochengine::scene
{
    using Vec3 = std::array<float, 3>;
    using SceneObjectId = std::uint64_t;

    inline constexpr std::uint32_t kSceneDocumentFormatVersion = 8u;
    inline constexpr SceneObjectId kInvalidSceneObjectId = 0u;
    inline constexpr Vec3 kDefaultAmbientLight{0.16f, 0.18f, 0.22f};

    [[nodiscard]] inline bool valid_scene_ambient(const Vec3& ambient) noexcept
    {
        return std::all_of(ambient.begin(), ambient.end(), [](float channel) noexcept
        {
            return std::isfinite(channel) && channel >= 0.0f;
        });
    }

    enum class SceneTextureFilter : std::uint8_t
    {
        nearest,
        linear
    };

    enum class SceneTextureAddress : std::uint8_t
    {
        clamp_to_edge,
        repeat,
        mirrored_repeat
    };

    enum class SceneTextureAlphaMode : std::uint8_t
    {
        opaque,
        straight,
        premultiplied,
        cutout
    };

    enum class SceneTextureColorSpace : std::uint8_t
    {
        linear,
        srgb
    };

    [[nodiscard]] inline bool valid_scene_asset_path(
        std::string_view path) noexcept
    {
        if (path.empty() || path.front() == '/' || path.front() == '\\'
            || path.find('\\') != std::string_view::npos
            || path.find(':') != std::string_view::npos)
        {
            return false;
        }

        std::size_t begin = 0;
        while (begin < path.size())
        {
            const std::size_t end = path.find('/', begin);
            const std::string_view segment = path.substr(
                begin,
                end == std::string_view::npos
                    ? path.size() - begin
                    : end - begin);
            if (segment.empty() || segment == "." || segment == "..")
                return false;
            for (const unsigned char character : segment)
            {
                if (character < 0x20u || character == 0x7fu)
                    return false;
            }
            if (end == std::string_view::npos)
                break;
            begin = end + 1u;
        }
        return true;
    }

    struct SceneTextureMaterialSnapshot final
    {
        std::string logical_path{};
        std::array<std::uint64_t, 4> artifact_key{};
        std::uint64_t stable_key{};
        SceneTextureFilter filter{SceneTextureFilter::linear};
        SceneTextureAddress address_u{SceneTextureAddress::clamp_to_edge};
        SceneTextureAddress address_v{SceneTextureAddress::clamp_to_edge};
        SceneTextureAlphaMode alpha{SceneTextureAlphaMode::straight};
        SceneTextureColorSpace color_space{SceneTextureColorSpace::srgb};
        float alpha_cutoff{0.5f};

        [[nodiscard]] bool valid() const noexcept
        {
            const bool key_present = std::any_of(
                artifact_key.begin(),
                artifact_key.end(),
                [](std::uint64_t word) { return word != 0u; });
            return valid_scene_asset_path(logical_path)
                && key_present
                && stable_key != 0u
                && static_cast<std::uint8_t>(filter)
                    <= static_cast<std::uint8_t>(SceneTextureFilter::linear)
                && static_cast<std::uint8_t>(address_u)
                    <= static_cast<std::uint8_t>(SceneTextureAddress::mirrored_repeat)
                && static_cast<std::uint8_t>(address_v)
                    <= static_cast<std::uint8_t>(SceneTextureAddress::mirrored_repeat)
                && static_cast<std::uint8_t>(alpha)
                    <= static_cast<std::uint8_t>(SceneTextureAlphaMode::cutout)
                && static_cast<std::uint8_t>(color_space)
                    <= static_cast<std::uint8_t>(SceneTextureColorSpace::srgb)
                && std::isfinite(alpha_cutoff)
                && alpha_cutoff >= 0.0f
                && alpha_cutoff <= 1.0f;
        }

        friend bool operator==(
            const SceneTextureMaterialSnapshot&,
            const SceneTextureMaterialSnapshot&) noexcept = default;
    };

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

    struct SceneMeshCoordinates final
    {
        double meters_per_unit{1.0};
        // Imported asset basis/provenance. Authored object position remains
        // independent; consumers must not add this origin to its transform.
        std::array<double, 3> source_origin_meters{};

        [[nodiscard]] bool valid() const noexcept
        {
            return std::isfinite(meters_per_unit)
                && meters_per_unit > 0.0 && meters_per_unit <= 1.0e12
                && std::all_of(source_origin_meters.begin(), source_origin_meters.end(),
                    [](double value) noexcept
                    {
                        return std::isfinite(value) && std::abs(value) <= 1.0e15;
                    });
        }

        friend bool operator==(
            const SceneMeshCoordinates&, const SceneMeshCoordinates&) noexcept = default;
    };

    struct SceneMeshSnapshot final
    {
        std::string logical_path{};
        std::array<std::uint64_t, 4> artifact_key{};
        std::uint32_t vertex_count{}, index_count{};
        std::optional<SceneMeshCoordinates> coordinates{};
        [[nodiscard]] bool valid() const noexcept
        {
            return valid_scene_asset_path(logical_path)
                && logical_path.back() != '/'
                && (logical_path.starts_with("Assets/") || logical_path.starts_with("Library/"))
                && std::any_of(artifact_key.begin(), artifact_key.end(), [](auto word) { return word != 0u; })
                // These are reference-format ceilings, not permission to load
                // geometry. Artifact decoding and frame residency still need
                // their caller's explicit byte/count budgets.
                && vertex_count >= 3u && vertex_count <= 1'048'576u
                && index_count >= 3u && index_count <= 6'291'456u && index_count % 3u == 0u
                && (!coordinates || coordinates->valid());
        }
        friend bool operator==(const SceneMeshSnapshot&, const SceneMeshSnapshot&) noexcept = default;
    };

    [[nodiscard]] inline std::optional<Vec3> mesh_render_scale(
        const Vec3& authored_scale,
        const std::optional<SceneMeshSnapshot>& mesh) noexcept
    {
        const SceneMeshCoordinates* coordinates = mesh && mesh->coordinates
            ? &*mesh->coordinates : nullptr;
        if (coordinates && !coordinates->valid())
            return std::nullopt;
        const double metersPerUnit = coordinates ? coordinates->meters_per_unit : 1.0;
        Vec3 result{};
        for (std::size_t axis = 0u; axis < result.size(); ++axis)
        {
            const double value = static_cast<double>(authored_scale[axis]) * metersPerUnit;
            if (!std::isfinite(value)
                || std::abs(value) > static_cast<double>((std::numeric_limits<float>::max)()))
                return std::nullopt;
            result[axis] = static_cast<float>(value);
            if (authored_scale[axis] != 0.0f && result[axis] == 0.0f)
                return std::nullopt;
        }
        return result;
    }

    // Per-instance region provenance travels through the ordinary scene object
    // transaction. A region is the group sharing region_id; no second scene store.
    struct SceneForestRegionInstance final
    {
        std::uint64_t region_id{}, terrain_object_id{}, seed{}, plant_genome{}, plant_revision{}, plant_hash{};
        std::uint32_t body_index{}, candidate_index{}, candidate_count{};
        double radius_meters{}, density_per_square_meter{}, body_radius_meters{};
        std::array<double,3> center_direction{};
        std::array<double,3> body_local_root_meters{}, radial_up{0,1,0};
        std::array<double,3> admitted_body_center_meters{}, mesh_source_origin_meters{};
        std::array<std::uint64_t,4> terrain_artifact_key{};
        std::string provider_content_hash{}, recipe{}, plant_source{};
        [[nodiscard]] bool valid() const noexcept
        {
            const auto length=std::hypot(center_direction[0],center_direction[1],center_direction[2]);
            const auto upLength=std::hypot(radial_up[0],radial_up[1],radial_up[2]);
            const auto rootLength=std::hypot(body_local_root_meters[0],body_local_root_meters[1],body_local_root_meters[2]);
            const auto bounded=[](const auto& values){return std::all_of(values.begin(),values.end(),
                [](double value){return std::isfinite(value)&&std::abs(value)<=1e15;});};
            return std::isfinite(upLength)&&std::abs(upLength-1.0)<1e-8
                && bounded(body_local_root_meters)&&std::isfinite(rootLength)&&rootLength>0
                && bounded(admitted_body_center_meters)&&bounded(mesh_source_origin_meters)
                && region_id && terrain_object_id && plant_genome && plant_revision && plant_hash
                && body_index<=5u && candidate_count>0u && candidate_count<=64u && candidate_index<candidate_count
                && std::isfinite(radius_meters) && radius_meters>=0.25 && radius_meters<=1000.0
                && std::isfinite(density_per_square_meter) && density_per_square_meter>0 && density_per_square_meter<=1.0
                && std::isfinite(body_radius_meters) && body_radius_meters>radius_meters && body_radius_meters<=1e10
                && std::isfinite(length) && std::abs(length-1.0)<1e-8
                && std::any_of(terrain_artifact_key.begin(),terrain_artifact_key.end(),[](auto value){return value!=0u;})
                && provider_content_hash.size()==64u
                && std::all_of(provider_content_hash.begin(),provider_content_hash.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');})
                && !recipe.empty() && recipe.size()<=1024u && valid_scene_asset_path(plant_source)
                && plant_source.size()<=4096u;
        }
        friend bool operator==(const SceneForestRegionInstance&,const SceneForestRegionInstance&) = default;
    };

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
        std::optional<SceneTextureMaterialSnapshot> texture_material{};
        std::optional<SceneMeshSnapshot> mesh_asset{};
        // Optional exact instance pivot. Position stays an editable local delta;
        // this never changes the asset's source/provenance coordinate frame.
        std::optional<std::array<double, 3>> position_anchor{};
        std::optional<SceneForestRegionInstance> forest_region{};
    };

    [[nodiscard]] inline std::optional<std::array<double, 3>> world_position(
        const Vec3& position,
        const std::optional<std::array<double, 3>>& anchor = {}) noexcept
    {
        std::array<double, 3> result{};
        for (std::size_t axis = 0u; axis < result.size(); ++axis)
        {
            if (!std::isfinite(position[axis]))
                return std::nullopt;
            const double base = anchor ? (*anchor)[axis] : 0.0;
            if (!std::isfinite(base) || (anchor && std::abs(base) > 1.0e15))
                return std::nullopt;
            result[axis] = base + static_cast<double>(position[axis]);
            if (!std::isfinite(result[axis]) || (anchor && std::abs(result[axis]) > 1.0e15))
                return std::nullopt;
        }
        return result;
    }

    [[nodiscard]] inline std::optional<std::array<double, 3>> world_position(
        const SceneObjectSnapshot& object) noexcept
    {
        return world_position(object.position, object.position_anchor);
    }

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
        Vec3 ambient{kDefaultAmbientLight};
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
        bool materials_valid{};
        bool meshes_valid{};
        bool object_names_unique{};
        bool transforms_valid{};
        bool timeline_valid{};
        bool packages_valid{};
        bool runtime_references_valid{};
        bool environment_valid{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return identity_valid
                && counts_bounded
                && strings_bounded
                && object_ids_valid
                && object_ids_unique
                && materials_valid
                && meshes_valid
                && object_names_unique
                && transforms_valid
                && timeline_valid
                && packages_valid
                && runtime_references_valid
                && environment_valid;
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
        result.environment_valid = valid_scene_ambient(snapshot.ambient);
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
        result.materials_valid = true;
        result.meshes_valid = true;
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
                && world_position(object).has_value()
                && finite_scene_vec3(object.rotation)
                && finite_scene_vec3(object.scale)
                && object.scale[0] > 0.0f
                && object.scale[1] > 0.0f
                && object.scale[2] > 0.0f;
            if (object.forest_region)
                result.meshes_valid = result.meshes_valid && object.forest_region->valid();
            if (object.mesh_asset)
            {
                result.strings_bounded = result.strings_bounded && bounded(object.mesh_asset->logical_path);
                result.meshes_valid = result.meshes_valid && object.mesh_asset->valid();
            }
            if (object.texture_material)
            {
                result.strings_bounded = result.strings_bounded
                    && bounded(object.texture_material->logical_path);
                result.materials_valid = result.materials_valid
                    && object.texture_material->valid();
            }
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
