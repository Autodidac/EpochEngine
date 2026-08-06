// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

export module scene.runtime;

import scenesnapshot;

export namespace epochengine::scene_runtime
{
    using SceneObjectId = epochengine::scene::SceneObjectId;
    using Vec3 = epochengine::scene::Vec3;

    inline constexpr std::uint32_t kInvalidRuntimeEntitySlot =
        (std::numeric_limits<std::uint32_t>::max)();
    inline constexpr std::size_t kDefaultMaximumRuntimeEntities = 65'536u;

    struct RuntimeEntityHandle final
    {
        std::uint32_t slot{kInvalidRuntimeEntitySlot};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return slot != kInvalidRuntimeEntitySlot && generation != 0u;
        }

        friend constexpr bool operator==(
            const RuntimeEntityHandle&,
            const RuntimeEntityHandle&) noexcept = default;
    };

    static_assert(!RuntimeEntityHandle{}.valid());
    static_assert(RuntimeEntityHandle{0u, 1u}.valid());

    enum class RuntimeEntityFlag : std::uint8_t
    {
        none = 0u,
        visible = 1u << 0u,
        editor_only = 1u << 1u
    };

    [[nodiscard]] constexpr RuntimeEntityFlag operator|(
        RuntimeEntityFlag lhs,
        RuntimeEntityFlag rhs) noexcept
    {
        return static_cast<RuntimeEntityFlag>(
            static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
    }

    [[nodiscard]] constexpr bool has_flag(
        RuntimeEntityFlag value,
        RuntimeEntityFlag flag) noexcept
    {
        return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0u;
    }

    struct RuntimeTransform final
    {
        Vec3 position{0.0f, 0.0f, 0.0f};
        Vec3 rotation{0.0f, 0.0f, 0.0f};
        Vec3 scale{1.0f, 1.0f, 1.0f};
    };

    struct RuntimeEntity final
    {
        SceneObjectId source_id{epochengine::scene::kInvalidSceneObjectId};
        RuntimeEntityHandle handle{};
        std::string name{};
        std::string type{};
        std::string category{};
        RuntimeTransform transform{};
        RuntimeEntityFlag flags{RuntimeEntityFlag::none};

        [[nodiscard]] constexpr bool visible() const noexcept
        {
            return has_flag(flags, RuntimeEntityFlag::visible);
        }

        [[nodiscard]] constexpr bool editor_only() const noexcept
        {
            return has_flag(flags, RuntimeEntityFlag::editor_only);
        }
    };

    enum class NeutralLightKind : std::uint8_t
    {
        generic,
        directional,
        point,
        spot,
        area
    };

    struct NeutralLightRecord final
    {
        SceneObjectId source_id{epochengine::scene::kInvalidSceneObjectId};
        RuntimeEntityHandle entity{};
        NeutralLightKind kind{NeutralLightKind::generic};
        std::string semantic_type{};
        Vec3 position{0.0f, 0.0f, 0.0f};
        Vec3 rotation{0.0f, 0.0f, 0.0f};
    };

    struct SourceSceneEvidence final
    {
        std::uint32_t format_version{epochengine::scene::kSceneDocumentFormatVersion};
        std::string project_id{};
        std::string scene_id{};
        std::string world_name{};
        std::string document_kind{};
        std::string support_tier{};
        std::uint64_t revision{};
        std::uint64_t captured_frame_index{};
        double captured_simulated_seconds{};
        std::size_t source_object_count{};
        SceneObjectId primary_camera{epochengine::scene::kInvalidSceneObjectId};
        SceneObjectId primary_spawn{epochengine::scene::kInvalidSceneObjectId};
    };

    struct RuntimePrimaryReferences final
    {
        SceneObjectId camera_id{epochengine::scene::kInvalidSceneObjectId};
        RuntimeEntityHandle camera{};
        SceneObjectId spawn_id{epochengine::scene::kInvalidSceneObjectId};
        RuntimeEntityHandle spawn{};
    };

    struct RuntimeScenePolicy final
    {
        std::size_t maximum_runtime_entities{kDefaultMaximumRuntimeEntities};
        bool include_hidden_objects{};
        bool include_editor_only_objects{};
        epochengine::scene::SceneDocumentLimits document_limits{};
    };

    enum class RuntimeSceneCompileStatus : std::uint8_t
    {
        ready,
        invalid_runnable_snapshot,
        invalid_policy,
        runtime_entity_limit_exceeded,
        generation_exhausted,
        primary_camera_excluded,
        primary_spawn_excluded
    };

    struct RuntimeSceneCompileReport final
    {
        RuntimeSceneCompileStatus status{RuntimeSceneCompileStatus::invalid_runnable_snapshot};
        epochengine::scene::SceneDocumentValidation source_validation{};
        std::uint64_t source_revision{};
        std::uint64_t committed_revision{};
        std::uint32_t committed_generation{};
        std::size_t source_entity_count{};
        std::size_t runtime_entity_count{};
        std::size_t excluded_hidden_count{};
        std::size_t excluded_editor_only_count{};
        std::size_t light_count{};

        [[nodiscard]] constexpr bool committed() const noexcept
        {
            return status == RuntimeSceneCompileStatus::ready;
        }
    };

    namespace detail
    {
        [[nodiscard]] constexpr char ascii_lower(char value) noexcept
        {
            return value >= 'A' && value <= 'Z'
                ? static_cast<char>(value + ('a' - 'A'))
                : value;
        }

        [[nodiscard]] constexpr bool ascii_equal(
            std::string_view lhs,
            std::string_view rhs) noexcept
        {
            if (lhs.size() != rhs.size())
                return false;
            for (std::size_t index = 0u; index < lhs.size(); ++index)
            {
                if (ascii_lower(lhs[index]) != ascii_lower(rhs[index]))
                    return false;
            }
            return true;
        }

        [[nodiscard]] constexpr bool ascii_contains(
            std::string_view value,
            std::string_view needle) noexcept
        {
            if (needle.empty() || needle.size() > value.size())
                return false;
            for (std::size_t offset = 0u; offset + needle.size() <= value.size(); ++offset)
            {
                bool matches = true;
                for (std::size_t index = 0u; index < needle.size(); ++index)
                {
                    if (ascii_lower(value[offset + index]) != ascii_lower(needle[index]))
                    {
                        matches = false;
                        break;
                    }
                }
                if (matches)
                    return true;
            }
            return false;
        }

        [[nodiscard]] constexpr std::optional<NeutralLightKind> classify_light(
            std::string_view type,
            std::string_view category) noexcept
        {
            if (ascii_contains(type, "directional") || ascii_equal(type, "sun"))
                return NeutralLightKind::directional;
            if (ascii_contains(type, "pointlight") || ascii_equal(type, "point light"))
                return NeutralLightKind::point;
            if (ascii_contains(type, "spotlight") || ascii_equal(type, "spot light"))
                return NeutralLightKind::spot;
            if (ascii_contains(type, "arealight") || ascii_equal(type, "area light"))
                return NeutralLightKind::area;
            if (ascii_contains(type, "light") || ascii_equal(category, "lighting"))
                return NeutralLightKind::generic;
            return std::nullopt;
        }

        [[nodiscard]] constexpr RuntimeEntityFlag source_flags(
            const epochengine::scene::SceneObjectSnapshot& source) noexcept
        {
            RuntimeEntityFlag result = RuntimeEntityFlag::none;
            if (source.visible)
                result = result | RuntimeEntityFlag::visible;
            if (source.editor_only)
                result = result | RuntimeEntityFlag::editor_only;
            return result;
        }

    }

    class SceneRuntime;

    class CompiledSceneProjection final
    {
    public:
        [[nodiscard]] bool ready() const noexcept
        {
            return ready_;
        }

        [[nodiscard]] const SourceSceneEvidence& source() const noexcept
        {
            return source_;
        }

        [[nodiscard]] const RuntimePrimaryReferences& primary() const noexcept
        {
            return primary_;
        }

        [[nodiscard]] std::span<const RuntimeEntity> entities() const noexcept
        {
            return entities_;
        }

        [[nodiscard]] std::span<const NeutralLightRecord> lights() const noexcept
        {
            return lights_;
        }

        [[nodiscard]] const RuntimeEntity* find(SceneObjectId source_id) const noexcept
        {
            const auto found = source_lookup_.find(source_id);
            if (found == source_lookup_.end() || found->second >= entities_.size())
                return nullptr;
            return &entities_[found->second];
        }

        [[nodiscard]] const RuntimeEntity* resolve(RuntimeEntityHandle handle) const noexcept
        {
            if (!handle.valid() || handle.generation != generation_
                || handle.slot >= entities_.size())
            {
                return nullptr;
            }
            const RuntimeEntity& entity = entities_[handle.slot];
            return entity.handle == handle ? &entity : nullptr;
        }

        [[nodiscard]] const NeutralLightRecord* find_light(SceneObjectId source_id) const noexcept
        {
            const auto found = light_lookup_.find(source_id);
            if (found == light_lookup_.end() || found->second >= lights_.size())
                return nullptr;
            return &lights_[found->second];
        }

        [[nodiscard]] std::uint32_t generation() const noexcept
        {
            return generation_;
        }

    private:
        friend class SceneRuntime;

        bool ready_{};
        std::uint32_t generation_{};
        SourceSceneEvidence source_{};
        RuntimePrimaryReferences primary_{};
        std::vector<RuntimeEntity> entities_{};
        std::vector<NeutralLightRecord> lights_{};
        std::unordered_map<SceneObjectId, std::size_t> source_lookup_{};
        std::unordered_map<SceneObjectId, std::size_t> light_lookup_{};
    };

    class SceneRuntime final
    {
    public:
        [[nodiscard]] RuntimeSceneCompileReport replace(
            const epochengine::scene::SceneSnapshot& snapshot,
            const RuntimeScenePolicy& policy = {})
        {
            RuntimeSceneCompileReport report{};
            report.source_revision = snapshot.revision;
            report.source_entity_count = snapshot.objects.size();
            if (active_.has_value())
            {
                report.committed_revision = active_->source().revision;
                report.committed_generation = active_->generation();
                report.runtime_entity_count = active_->entities().size();
                report.light_count = active_->lights().size();
            }

            report.source_validation = epochengine::scene::validate_scene_document(
                snapshot,
                epochengine::scene::SceneDocumentRequirement::runnable,
                policy.document_limits);
            if (!report.source_validation)
            {
                report.status = RuntimeSceneCompileStatus::invalid_runnable_snapshot;
                return report;
            }
            if (policy.maximum_runtime_entities == 0u
                || policy.maximum_runtime_entities > kDefaultMaximumRuntimeEntities)
            {
                report.status = RuntimeSceneCompileStatus::invalid_policy;
                return report;
            }
            if (generation_ == (std::numeric_limits<std::uint32_t>::max)())
            {
                report.status = RuntimeSceneCompileStatus::generation_exhausted;
                return report;
            }

            CompiledSceneProjection candidate{};
            candidate.generation_ = generation_ + 1u;
            candidate.source_ = SourceSceneEvidence{
                .format_version = epochengine::scene::kSceneDocumentFormatVersion,
                .project_id = snapshot.project_id,
                .scene_id = snapshot.scene_id,
                .world_name = snapshot.world_name,
                .document_kind = snapshot.document_kind,
                .support_tier = snapshot.support_tier,
                .revision = snapshot.revision,
                .captured_frame_index = snapshot.captured_frame_index,
                .captured_simulated_seconds = snapshot.captured_simulated_seconds,
                .source_object_count = snapshot.objects.size(),
                .primary_camera = snapshot.primary_camera,
                .primary_spawn = snapshot.primary_spawn
            };

            std::vector<const epochengine::scene::SceneObjectSnapshot*> included{};
            included.reserve(snapshot.objects.size());
            for (const epochengine::scene::SceneObjectSnapshot& source : snapshot.objects)
            {
                const bool hidden_excluded =
                    !source.visible && !policy.include_hidden_objects;
                const bool editor_only_excluded =
                    source.editor_only && !policy.include_editor_only_objects;
                if (hidden_excluded)
                    ++report.excluded_hidden_count;
                if (editor_only_excluded)
                    ++report.excluded_editor_only_count;
                if (!hidden_excluded && !editor_only_excluded)
                    included.push_back(&source);
            }
            if (included.size() > policy.maximum_runtime_entities)
            {
                report.status = RuntimeSceneCompileStatus::runtime_entity_limit_exceeded;
                return report;
            }

            std::sort(
                included.begin(),
                included.end(),
                [](const auto* lhs, const auto* rhs)
                {
                    return lhs->id < rhs->id;
                });

            candidate.entities_.reserve(included.size());
            candidate.source_lookup_.reserve(included.size());
            for (std::size_t index = 0u; index < included.size(); ++index)
            {
                const auto& source = *included[index];
                const RuntimeEntityHandle handle{
                    static_cast<std::uint32_t>(index),
                    candidate.generation_
                };
                candidate.source_lookup_.emplace(source.id, index);
                candidate.entities_.push_back(RuntimeEntity{
                    .source_id = source.id,
                    .handle = handle,
                    .name = source.name,
                    .type = source.type,
                    .category = source.category,
                    .transform = RuntimeTransform{
                        .position = source.position,
                        .rotation = source.rotation,
                        .scale = source.scale
                    },
                    .flags = detail::source_flags(source)
                });

                if (const auto kind = detail::classify_light(source.type, source.category))
                {
                    candidate.light_lookup_.emplace(source.id, candidate.lights_.size());
                    candidate.lights_.push_back(NeutralLightRecord{
                        .source_id = source.id,
                        .entity = handle,
                        .kind = *kind,
                        .semantic_type = source.type,
                        .position = source.position,
                        .rotation = source.rotation
                    });
                }
            }

            const RuntimeEntity* camera = candidate.find(snapshot.primary_camera);
            if (camera == nullptr)
            {
                report.status = RuntimeSceneCompileStatus::primary_camera_excluded;
                return report;
            }
            const RuntimeEntity* spawn = candidate.find(snapshot.primary_spawn);
            if (spawn == nullptr)
            {
                report.status = RuntimeSceneCompileStatus::primary_spawn_excluded;
                return report;
            }
            candidate.primary_ = RuntimePrimaryReferences{
                .camera_id = camera->source_id,
                .camera = camera->handle,
                .spawn_id = spawn->source_id,
                .spawn = spawn->handle
            };
            candidate.ready_ = true;

            generation_ = candidate.generation_;
            active_ = std::move(candidate);
            report.status = RuntimeSceneCompileStatus::ready;
            report.committed_revision = active_->source().revision;
            report.committed_generation = active_->generation();
            report.runtime_entity_count = active_->entities().size();
            report.light_count = active_->lights().size();
            return report;
        }

        [[nodiscard]] bool ready() const noexcept
        {
            return active_.has_value() && active_->ready();
        }

        [[nodiscard]] const CompiledSceneProjection* projection() const noexcept
        {
            return active_.has_value() ? &*active_ : nullptr;
        }

        [[nodiscard]] const RuntimeEntity* find(SceneObjectId source_id) const noexcept
        {
            return active_.has_value() ? active_->find(source_id) : nullptr;
        }

        [[nodiscard]] const RuntimeEntity* resolve(RuntimeEntityHandle handle) const noexcept
        {
            return active_.has_value() ? active_->resolve(handle) : nullptr;
        }

    private:
        std::uint32_t generation_{};
        std::optional<CompiledSceneProjection> active_{};
    };

    struct SceneRuntimeContractReport final
    {
        bool reorder_stable_identity{};
        bool stale_handle_rejected{};
        bool exact_source_revision{};
        bool runtime_exclusion_enforced{};
        bool primary_references_resolved{};
        bool neutral_lights_compiled{};
        bool invalid_runnable_rejected_without_fallback{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return reorder_stable_identity
                && stale_handle_rejected
                && exact_source_revision
                && runtime_exclusion_enforced
                && primary_references_resolved
                && neutral_lights_compiled
                && invalid_runnable_rejected_without_fallback;
        }
    };

    [[nodiscard]] inline SceneRuntimeContractReport run_scene_runtime_contract_checks()
    {
        constexpr SceneObjectId camera_id = 101u;
        constexpr SceneObjectId spawn_id = 202u;
        constexpr SceneObjectId mesh_id = 303u;
        constexpr SceneObjectId hidden_id = 404u;
        constexpr SceneObjectId editor_id = 505u;
        constexpr SceneObjectId light_id = 606u;

        epochengine::scene::SceneSnapshot snapshot{};
        snapshot.scene_id = "scene.runtime.contract";
        snapshot.project_id = "project.runtime.contract";
        snapshot.world_name = "Runtime Contract";
        snapshot.revision = 77u;
        snapshot.primary_camera = camera_id;
        snapshot.primary_spawn = spawn_id;
        snapshot.objects = {
            epochengine::scene::SceneObjectSnapshot{
                .id = mesh_id,
                .name = "Mesh",
                .type = "Mesh",
                .category = "Geometry"
            },
            epochengine::scene::SceneObjectSnapshot{
                .id = camera_id,
                .name = "Camera",
                .type = "Camera",
                .category = "Scene"
            },
            epochengine::scene::SceneObjectSnapshot{
                .id = hidden_id,
                .name = "Hidden",
                .type = "Mesh",
                .category = "Geometry",
                .visible = false
            },
            epochengine::scene::SceneObjectSnapshot{
                .id = light_id,
                .name = "KeyLight",
                .type = "DirectionalLight",
                .category = "Lighting",
                .rotation = {-35.0f, 25.0f, 0.0f}
            },
            epochengine::scene::SceneObjectSnapshot{
                .id = editor_id,
                .name = "EditorGuide",
                .type = "Guide",
                .category = "Editor",
                .editor_only = true
            },
            epochengine::scene::SceneObjectSnapshot{
                .id = spawn_id,
                .name = "Spawn",
                .type = "Spawn",
                .category = "Gameplay",
                .position = {0.0f, 1.0f, 0.0f}
            }
        };

        SceneRuntimeContractReport result{};
        SceneRuntime runtime{};
        const RuntimeSceneCompileReport first = runtime.replace(snapshot);
        const CompiledSceneProjection* first_projection = runtime.projection();
        const RuntimeEntity* first_mesh = runtime.find(mesh_id);
        const RuntimeEntityHandle stale_mesh = first_mesh != nullptr
            ? first_mesh->handle
            : RuntimeEntityHandle{};

        epochengine::scene::SceneSnapshot reordered = snapshot;
        std::reverse(reordered.objects.begin(), reordered.objects.end());
        SceneRuntime reordered_runtime{};
        const RuntimeSceneCompileReport reordered_build = reordered_runtime.replace(reordered);
        const RuntimeEntity* reordered_mesh = reordered_runtime.find(mesh_id);
        result.reorder_stable_identity = first.committed()
            && reordered_build.committed()
            && first_mesh != nullptr
            && reordered_mesh != nullptr
            && first_mesh->source_id == reordered_mesh->source_id
            && first_mesh->handle == reordered_mesh->handle;

        result.exact_source_revision = first_projection != nullptr
            && first.source_revision == 77u
            && first.committed_revision == 77u
            && first_projection->source().revision == 77u
            && first_projection->source().scene_id == snapshot.scene_id;
        result.runtime_exclusion_enforced = first_projection != nullptr
            && first.runtime_entity_count == 4u
            && first.excluded_hidden_count == 1u
            && first.excluded_editor_only_count == 1u
            && first_projection->find(hidden_id) == nullptr
            && first_projection->find(editor_id) == nullptr;
        result.primary_references_resolved = first_projection != nullptr
            && first_projection->primary().camera_id == camera_id
            && first_projection->primary().spawn_id == spawn_id
            && first_projection->resolve(first_projection->primary().camera) != nullptr
            && first_projection->resolve(first_projection->primary().spawn) != nullptr;
        const NeutralLightRecord* light = first_projection != nullptr
            ? first_projection->find_light(light_id)
            : nullptr;
        result.neutral_lights_compiled = first.light_count == 1u
            && light != nullptr
            && light->kind == NeutralLightKind::directional
            && first_projection->resolve(light->entity) != nullptr;

        snapshot.revision = 78u;
        const RuntimeSceneCompileReport replacement = runtime.replace(snapshot);
        const RuntimeEntity* current_mesh = runtime.find(mesh_id);
        result.stale_handle_rejected = replacement.committed()
            && stale_mesh.valid()
            && runtime.resolve(stale_mesh) == nullptr
            && current_mesh != nullptr
            && current_mesh->handle.generation != stale_mesh.generation;

        epochengine::scene::SceneSnapshot invalid = snapshot;
        invalid.revision = 79u;
        invalid.primary_camera = 999'999u;
        SceneRuntime empty_runtime{};
        const RuntimeSceneCompileReport empty_failure = empty_runtime.replace(invalid);
        const RuntimeSceneCompileReport retained_failure = runtime.replace(invalid);
        const CompiledSceneProjection* retained = runtime.projection();
        result.invalid_runnable_rejected_without_fallback =
            empty_failure.status == RuntimeSceneCompileStatus::invalid_runnable_snapshot
            && !empty_runtime.ready()
            && retained_failure.status == RuntimeSceneCompileStatus::invalid_runnable_snapshot
            && retained != nullptr
            && retained->source().revision == 78u
            && retained->primary().camera_id == camera_id
            && retained->find(999'999u) == nullptr;
        return result;
    }
}
