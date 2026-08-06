// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

export module scene.interaction;

import scenesnapshot;
import render.ray;

export namespace epochengine::scene_interaction
{
    using Vec3 = epochengine::ray::Vec3;

    inline constexpr std::uint32_t kAllVisibility =
        (std::numeric_limits<std::uint32_t>::max)();
    inline constexpr std::size_t kMaximumSceneObjects = 65'536;
    inline constexpr std::size_t kMaximumIdentityBytes = 4'096;

    struct SceneObjectId final
    {
        std::uint64_t value{};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return value != 0 && generation != 0;
        }

        friend constexpr bool operator==(const SceneObjectId&, const SceneObjectId&) noexcept = default;
    };

    struct SceneObjectIdHash final
    {
        [[nodiscard]] std::size_t operator()(SceneObjectId id) const noexcept
        {
            std::uint64_t value = id.value;
            value ^= static_cast<std::uint64_t>(id.generation) +
                0x9e3779b97f4a7c15ull + (value << 6u) + (value >> 2u);
            if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t))
            {
                value ^= value >> 32u;
            }
            return static_cast<std::size_t>(value);
        }
    };

    [[nodiscard]] constexpr epochengine::ray::QueryObjectId to_query_object_id(
        SceneObjectId object) noexcept
    {
        return {object.value, object.generation};
    }

    [[nodiscard]] constexpr SceneObjectId from_query_object_id(
        epochengine::ray::QueryObjectId object) noexcept
    {
        return {object.value, object.generation};
    }

    namespace detail
    {
        inline constexpr float kVectorEpsilonSquared = 1.0e-12f;

        [[nodiscard]] constexpr Vec3 add(Vec3 lhs, Vec3 rhs) noexcept
        {
            return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
        }

        [[nodiscard]] constexpr Vec3 subtract(Vec3 lhs, Vec3 rhs) noexcept
        {
            return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
        }

        [[nodiscard]] constexpr Vec3 scale(Vec3 value, float factor) noexcept
        {
            return {value.x * factor, value.y * factor, value.z * factor};
        }

        [[nodiscard]] constexpr float dot(Vec3 lhs, Vec3 rhs) noexcept
        {
            return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
        }

        [[nodiscard]] constexpr Vec3 cross(Vec3 lhs, Vec3 rhs) noexcept
        {
            return {
                lhs.y * rhs.z - lhs.z * rhs.y,
                lhs.z * rhs.x - lhs.x * rhs.z,
                lhs.x * rhs.y - lhs.y * rhs.x
            };
        }

        [[nodiscard]] inline bool finite(Vec3 value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        [[nodiscard]] constexpr float length_squared(Vec3 value) noexcept
        {
            return detail::dot(value, value);
        }

        [[nodiscard]] inline float length(Vec3 value) noexcept
        {
            return std::sqrt(detail::length_squared(value));
        }

        [[nodiscard]] inline std::optional<Vec3> normalized(Vec3 value) noexcept
        {
            const float magnitudeSquared = detail::length_squared(value);
            if (!detail::finite(value) || !std::isfinite(magnitudeSquared) ||
                magnitudeSquared <= kVectorEpsilonSquared)
            {
                return std::nullopt;
            }
            return detail::scale(value, 1.0f / std::sqrt(magnitudeSquared));
        }

        [[nodiscard]] inline Vec3 rotate_xyz_degrees(
            Vec3 value,
            const epochengine::scene::Vec3& rotation) noexcept
        {
            const float factor = std::numbers::pi_v<float> / 180.0f;
            const float x = rotation[0] * factor;
            const float y = rotation[1] * factor;
            const float z = rotation[2] * factor;

            const float sinX = std::sin(x);
            const float cosX = std::cos(x);
            const float sinY = std::sin(y);
            const float cosY = std::cos(y);
            const float sinZ = std::sin(z);
            const float cosZ = std::cos(z);

            const Vec3 aroundX{
                value.x,
                value.y * cosX - value.z * sinX,
                value.y * sinX + value.z * cosX
            };
            const Vec3 aroundY{
                aroundX.x * cosY + aroundX.z * sinY,
                aroundX.y,
                -aroundX.x * sinY + aroundX.z * cosY
            };
            return {
                aroundY.x * cosZ - aroundY.y * sinZ,
                aroundY.x * sinZ + aroundY.y * cosZ,
                aroundY.z
            };
        }

        [[nodiscard]] inline std::optional<epochengine::ray::Aabb> world_bounds(
            const epochengine::scene::SceneObjectSnapshot& object,
            const epochengine::ray::Aabb& localBounds,
            float minimumHalfExtent) noexcept
        {
            if (!epochengine::ray::validate(localBounds) ||
                !std::isfinite(minimumHalfExtent) || minimumHalfExtent < 0.0f)
            {
                return std::nullopt;
            }
            for (const float value : object.position)
            {
                if (!std::isfinite(value))
                {
                    return std::nullopt;
                }
            }
            for (const float value : object.rotation)
            {
                if (!std::isfinite(value))
                {
                    return std::nullopt;
                }
            }
            for (const float value : object.scale)
            {
                if (!std::isfinite(value))
                {
                    return std::nullopt;
                }
            }

            const Vec3 position{object.position[0], object.position[1], object.position[2]};
            Vec3 minimum{
                (std::numeric_limits<float>::max)(),
                (std::numeric_limits<float>::max)(),
                (std::numeric_limits<float>::max)()
            };
            Vec3 maximum{
                -(std::numeric_limits<float>::max)(),
                -(std::numeric_limits<float>::max)(),
                -(std::numeric_limits<float>::max)()
            };

            for (std::uint32_t corner = 0; corner < 8; ++corner)
            {
                Vec3 local{
                    (corner & 1u) != 0 ? localBounds.maximum.x : localBounds.minimum.x,
                    (corner & 2u) != 0 ? localBounds.maximum.y : localBounds.minimum.y,
                    (corner & 4u) != 0 ? localBounds.maximum.z : localBounds.minimum.z
                };
                local.x *= object.scale[0];
                local.y *= object.scale[1];
                local.z *= object.scale[2];
                const Vec3 world = detail::add(
                    position,
                    detail::rotate_xyz_degrees(local, object.rotation));
                if (!detail::finite(world))
                {
                    return std::nullopt;
                }
                minimum.x = (std::min)(minimum.x, world.x);
                minimum.y = (std::min)(minimum.y, world.y);
                minimum.z = (std::min)(minimum.z, world.z);
                maximum.x = (std::max)(maximum.x, world.x);
                maximum.y = (std::max)(maximum.y, world.y);
                maximum.z = (std::max)(maximum.z, world.z);
            }

            const Vec3 center = detail::scale(detail::add(minimum, maximum), 0.5f);
            const Vec3 halfExtent = detail::scale(detail::subtract(maximum, minimum), 0.5f);
            const Vec3 clampedHalfExtent{
                (std::max)(halfExtent.x, minimumHalfExtent),
                (std::max)(halfExtent.y, minimumHalfExtent),
                (std::max)(halfExtent.z, minimumHalfExtent)
            };
            epochengine::ray::Aabb result{
                detail::subtract(center, clampedHalfExtent),
                detail::add(center, clampedHalfExtent)
            };
            return epochengine::ray::validate(result)
                ? std::optional<epochengine::ray::Aabb>{result}
                : std::nullopt;
        }

        [[nodiscard]] constexpr epochengine::ray::Aabb merge(
            epochengine::ray::Aabb lhs,
            const epochengine::ray::Aabb& rhs) noexcept
        {
            lhs.minimum.x = (std::min)(lhs.minimum.x, rhs.minimum.x);
            lhs.minimum.y = (std::min)(lhs.minimum.y, rhs.minimum.y);
            lhs.minimum.z = (std::min)(lhs.minimum.z, rhs.minimum.z);
            lhs.maximum.x = (std::max)(lhs.maximum.x, rhs.maximum.x);
            lhs.maximum.y = (std::max)(lhs.maximum.y, rhs.maximum.y);
            lhs.maximum.z = (std::max)(lhs.maximum.z, rhs.maximum.z);
            return lhs;
        }
    }

    [[nodiscard]] inline std::optional<SceneObjectId> derive_scene_object_id(
        const epochengine::scene::SceneSnapshot& snapshot,
        const epochengine::scene::SceneObjectSnapshot& object) noexcept
    {
        if (object.id != epochengine::scene::kInvalidSceneObjectId)
        {
            return SceneObjectId{object.id, 1u};
        }

        const std::string_view sceneIdentity = snapshot.scene_id.empty()
            ? std::string_view{snapshot.world_name}
            : std::string_view{snapshot.scene_id};
        if (sceneIdentity.empty() || sceneIdentity.size() > kMaximumIdentityBytes ||
            object.name.empty() || object.name.size() > kMaximumIdentityBytes)
        {
            return std::nullopt;
        }
        return SceneObjectId{
            epochengine::scene::stable_scene_object_id(sceneIdentity, object.name),
            1u
        };
    }

    enum class Selectability : std::uint8_t
    {
        Inherit,
        Selectable,
        Unselectable
    };

    struct SceneObjectInteractionOverride final
    {
        SceneObjectId object{};
        epochengine::ray::Aabb localBounds{};
        std::uint32_t visibilityMask{kAllVisibility};
        Selectability selectability{Selectability::Inherit};
        bool useLocalBounds{};
        bool includeWhenHidden{};
    };

    struct SceneInteractionBuildOptions final
    {
        std::size_t maximumObjects{kMaximumSceneObjects};
        float minimumHalfExtent{0.025f};
        bool includeEditorOnly{true};
        bool hiddenObjectsSelectable{};
        std::uint32_t defaultVisibilityMask{kAllVisibility};
    };

    enum class SceneInteractionBuildStatus : std::uint8_t
    {
        Ready,
        InvalidSceneIdentity,
        TooManyObjects,
        IdentityCountMismatch,
        InvalidIdentity,
        DuplicateIdentity,
        InvalidOverride,
        DuplicateOverride,
        UnknownOverride,
        InvalidTransformOrBounds,
        RayPrimitiveRejected
    };

    struct SceneInteractionBuildReport final
    {
        SceneInteractionBuildStatus status{SceneInteractionBuildStatus::InvalidSceneIdentity};
        std::size_t sourceObjectCount{};
        std::size_t queryableObjectCount{};
        std::size_t hiddenObjectCount{};
        std::size_t unselectableObjectCount{};
        std::size_t editorOnlyObjectCount{};
        std::size_t failureIndex{(std::numeric_limits<std::size_t>::max)()};
        std::uint64_t sourceRevision{};
        std::uint64_t committedRevision{};
        std::uint64_t interactionRevision{};

        [[nodiscard]] constexpr bool committed() const noexcept
        {
            return status == SceneInteractionBuildStatus::Ready;
        }
    };

    struct SceneInteractionRecord final
    {
        SceneObjectId object{};
        std::size_t snapshotIndex{};
        epochengine::ray::Aabb worldBounds{};
        std::uint32_t visibilityMask{kAllVisibility};
        bool visible{};
        bool editorOnly{};
        bool selectable{};
        bool queryable{};
    };

    struct SceneSelectionHit final
    {
        SceneObjectId object{};
        std::size_t snapshotIndex{};
        epochengine::ray::Aabb worldBounds{};
        float distance{};
        Vec3 position{};
        Vec3 geometricNormal{};
        bool frontFace{};
    };

    struct SceneSelectionResult final
    {
        epochengine::ray::QueryStatus status{epochengine::ray::QueryStatus::Invalid};
        SceneSelectionHit hit{};
        std::uint32_t testedPrimitives{};
        std::uint64_t sceneRevision{};
        std::uint64_t interactionRevision{};

        [[nodiscard]] constexpr bool has_hit() const noexcept
        {
            return status == epochengine::ray::QueryStatus::Hit && hit.object.valid();
        }
    };

    struct FocusSettings final
    {
        float verticalFieldOfViewRadians{std::numbers::pi_v<float> / 3.0f};
        float aspectRatio{16.0f / 9.0f};
        float padding{1.25f};
        float minimumDistance{0.25f};
    };

    struct FocusTarget final
    {
        epochengine::ray::Aabb bounds{};
        Vec3 target{};
        float radius{};
        float suggestedDistance{};
        std::size_t objectCount{};
    };

    class SceneInteractionQuery final
    {
    public:
        SceneInteractionQuery()
            : rayScene_(std::make_unique<epochengine::ray::RayScene>())
        {
        }

        [[nodiscard]] SceneInteractionBuildReport rebuild(
            const epochengine::scene::SceneSnapshot& snapshot,
            SceneInteractionBuildOptions options = {},
            std::span<const SceneObjectInteractionOverride> overrides = {})
        {
            return rebuild(snapshot, {}, options, overrides);
        }

        [[nodiscard]] SceneInteractionBuildReport rebuild(
            const epochengine::scene::SceneSnapshot& snapshot,
            std::span<const SceneObjectId> stableObjectIds,
            SceneInteractionBuildOptions options = {},
            std::span<const SceneObjectInteractionOverride> overrides = {})
        {
            SceneInteractionBuildReport report{};
            report.sourceObjectCount = snapshot.objects.size();
            report.sourceRevision = snapshot.revision;
            report.committedRevision = sourceRevision_;
            report.interactionRevision = revision_;

            const std::string_view sceneIdentity = snapshot.scene_id.empty()
                ? std::string_view{snapshot.world_name}
                : std::string_view{snapshot.scene_id};
            if (sceneIdentity.empty() || snapshot.revision == 0)
            {
                report.status = SceneInteractionBuildStatus::InvalidSceneIdentity;
                return report;
            }
            if (options.maximumObjects == 0 ||
                options.maximumObjects > kMaximumSceneObjects ||
                snapshot.objects.size() > options.maximumObjects ||
                overrides.size() > options.maximumObjects)
            {
                report.status = SceneInteractionBuildStatus::TooManyObjects;
                return report;
            }
            if (!stableObjectIds.empty() && stableObjectIds.size() != snapshot.objects.size())
            {
                report.status = SceneInteractionBuildStatus::IdentityCountMismatch;
                return report;
            }
            if (!std::isfinite(options.minimumHalfExtent) || options.minimumHalfExtent < 0.0f ||
                options.defaultVisibilityMask == 0)
            {
                report.status = SceneInteractionBuildStatus::InvalidTransformOrBounds;
                return report;
            }

            std::unordered_map<SceneObjectId, const SceneObjectInteractionOverride*, SceneObjectIdHash>
                overrideByObject{};
            overrideByObject.reserve(overrides.size());
            for (std::size_t index = 0; index < overrides.size(); ++index)
            {
                const SceneObjectInteractionOverride& overrideValue = overrides[index];
                if (!overrideValue.object.valid() || overrideValue.visibilityMask == 0 ||
                    (overrideValue.useLocalBounds && !epochengine::ray::validate(overrideValue.localBounds)))
                {
                    report.status = SceneInteractionBuildStatus::InvalidOverride;
                    report.failureIndex = index;
                    return report;
                }
                if (!overrideByObject.emplace(overrideValue.object, &overrideValue).second)
                {
                    report.status = SceneInteractionBuildStatus::DuplicateOverride;
                    report.failureIndex = index;
                    return report;
                }
            }

            auto candidateRayScene = std::make_unique<epochengine::ray::RayScene>();
            std::vector<SceneInteractionRecord> candidateRecords{};
            candidateRecords.reserve(snapshot.objects.size());
            std::unordered_map<SceneObjectId, std::size_t, SceneObjectIdHash> candidateLookup{};
            candidateLookup.reserve(snapshot.objects.size());
            std::unordered_set<SceneObjectId, SceneObjectIdHash> consumedOverrides{};
            consumedOverrides.reserve(overrides.size());

            const epochengine::ray::Aabb defaultLocalBounds{};
            for (std::size_t index = 0; index < snapshot.objects.size(); ++index)
            {
                const auto& source = snapshot.objects[index];
                const std::optional<SceneObjectId> derived =
                    source.id != epochengine::scene::kInvalidSceneObjectId
                    ? std::optional<SceneObjectId>{SceneObjectId{source.id, 1u}}
                    : (stableObjectIds.empty()
                        ? derive_scene_object_id(snapshot, source)
                        : std::optional<SceneObjectId>{stableObjectIds[index]});
                if (!derived.has_value() || !derived->valid())
                {
                    report.status = SceneInteractionBuildStatus::InvalidIdentity;
                    report.failureIndex = index;
                    return report;
                }
                if (candidateLookup.contains(*derived))
                {
                    report.status = SceneInteractionBuildStatus::DuplicateIdentity;
                    report.failureIndex = index;
                    return report;
                }

                const SceneObjectInteractionOverride* overrideValue = nullptr;
                if (const auto found = overrideByObject.find(*derived); found != overrideByObject.end())
                {
                    overrideValue = found->second;
                    consumedOverrides.insert(*derived);
                }

                const epochengine::ray::Aabb& localBounds =
                    overrideValue != nullptr && overrideValue->useLocalBounds
                    ? overrideValue->localBounds
                    : defaultLocalBounds;
                const auto bounds = detail::world_bounds(
                    source,
                    localBounds,
                    options.minimumHalfExtent);
                if (!bounds.has_value())
                {
                    report.status = SceneInteractionBuildStatus::InvalidTransformOrBounds;
                    report.failureIndex = index;
                    return report;
                }

                bool selectable = overrideValue == nullptr ||
                    overrideValue->selectability != Selectability::Unselectable;
                if (overrideValue != nullptr &&
                    overrideValue->selectability == Selectability::Selectable)
                {
                    selectable = true;
                }
                const bool hidden = !source.visible;
                const bool hiddenAllowed = options.hiddenObjectsSelectable ||
                    (overrideValue != nullptr && overrideValue->includeWhenHidden);
                const bool editorOnlyAllowed = options.includeEditorOnly || !source.editor_only;
                const bool queryable = selectable && (!hidden || hiddenAllowed) && editorOnlyAllowed;
                const std::uint32_t visibilityMask = overrideValue != nullptr
                    ? overrideValue->visibilityMask
                    : options.defaultVisibilityMask;

                if (hidden)
                {
                    ++report.hiddenObjectCount;
                }
                if (!selectable)
                {
                    ++report.unselectableObjectCount;
                }
                if (source.editor_only)
                {
                    ++report.editorOnlyObjectCount;
                }

                const std::size_t recordIndex = candidateRecords.size();
                candidateLookup.emplace(*derived, recordIndex);
                candidateRecords.push_back({
                    *derived,
                    index,
                    *bounds,
                    visibilityMask,
                    source.visible,
                    source.editor_only,
                    selectable,
                    queryable
                });

                if (!queryable)
                {
                    continue;
                }

                epochengine::ray::PrimitiveDesc primitive{};
                primitive.kind = epochengine::ray::PrimitiveKind::Aabb;
                primitive.aabb = *bounds;
                primitive.visibilityMask = visibilityMask;
                primitive.object = to_query_object_id(*derived);
                primitive.primitiveIndex = 0;
                primitive.enabled = true;
                if (!candidateRayScene->create(primitive).valid())
                {
                    report.status = SceneInteractionBuildStatus::RayPrimitiveRejected;
                    report.failureIndex = index;
                    return report;
                }
                ++report.queryableObjectCount;
            }

            if (consumedOverrides.size() != overrides.size())
            {
                report.status = SceneInteractionBuildStatus::UnknownOverride;
                for (std::size_t index = 0; index < overrides.size(); ++index)
                {
                    if (!consumedOverrides.contains(overrides[index].object))
                    {
                        report.failureIndex = index;
                        break;
                    }
                }
                return report;
            }

            rayScene_.swap(candidateRayScene);
            records_.swap(candidateRecords);
            lookup_.swap(candidateLookup);
            sourceRevision_ = snapshot.revision;
            ++revision_;
            if (revision_ == 0)
            {
                revision_ = 1;
            }
            report.status = SceneInteractionBuildStatus::Ready;
            report.committedRevision = sourceRevision_;
            report.interactionRevision = revision_;
            return report;
        }

        [[nodiscard]] SceneSelectionResult trace(
            const epochengine::ray::RayDesc& ray,
            epochengine::ray::QueryMode mode = epochengine::ray::QueryMode::Closest) const noexcept
        {
            SceneSelectionResult result{};
            result.sceneRevision = sourceRevision_;
            result.interactionRevision = revision_;
            if (!rayScene_)
            {
                return result;
            }

            const epochengine::ray::RayQueryResult rayResult = rayScene_->trace(ray, mode);
            result.status = rayResult.status;
            result.testedPrimitives = rayResult.testedPrimitives;
            if (rayResult.status != epochengine::ray::QueryStatus::Hit)
            {
                return result;
            }

            const SceneObjectId object = from_query_object_id(rayResult.hit.object);
            const SceneInteractionRecord* record = resolve(object);
            if (record == nullptr || !record->queryable)
            {
                result.status = epochengine::ray::QueryStatus::Invalid;
                return result;
            }

            result.hit = {
                object,
                record->snapshotIndex,
                record->worldBounds,
                rayResult.hit.distance,
                rayResult.hit.position,
                rayResult.hit.geometricNormal,
                rayResult.hit.frontFace
            };
            return result;
        }

        [[nodiscard]] const SceneInteractionRecord* resolve(SceneObjectId object) const noexcept
        {
            const auto found = lookup_.find(object);
            if (found == lookup_.end() || found->second >= records_.size())
            {
                return nullptr;
            }
            return &records_[found->second];
        }

        [[nodiscard]] std::optional<epochengine::ray::Aabb> focus_bounds(
            SceneObjectId object) const noexcept
        {
            const SceneInteractionRecord* record = resolve(object);
            return record == nullptr
                ? std::nullopt
                : std::optional<epochengine::ray::Aabb>{record->worldBounds};
        }

        [[nodiscard]] std::optional<epochengine::ray::Aabb> focus_bounds(
            std::span<const SceneObjectId> objects) const noexcept
        {
            if (objects.empty())
            {
                return std::nullopt;
            }

            std::optional<epochengine::ray::Aabb> combined{};
            for (const SceneObjectId object : objects)
            {
                const SceneInteractionRecord* record = resolve(object);
                if (record == nullptr)
                {
                    return std::nullopt;
                }
                combined = combined.has_value()
                    ? std::optional<epochengine::ray::Aabb>{detail::merge(*combined, record->worldBounds)}
                    : std::optional<epochengine::ray::Aabb>{record->worldBounds};
            }
            return combined;
        }

        [[nodiscard]] std::optional<FocusTarget> focus_target(
            SceneObjectId object,
            FocusSettings settings = {}) const noexcept
        {
            const std::array<SceneObjectId, 1> objects{object};
            return focus_target(std::span<const SceneObjectId>{objects}, settings);
        }

        [[nodiscard]] std::optional<FocusTarget> focus_target(
            std::span<const SceneObjectId> objects,
            FocusSettings settings = {}) const noexcept
        {
            const auto bounds = focus_bounds(objects);
            if (!bounds.has_value() ||
                !std::isfinite(settings.verticalFieldOfViewRadians) ||
                !std::isfinite(settings.aspectRatio) ||
                !std::isfinite(settings.padding) ||
                !std::isfinite(settings.minimumDistance) ||
                settings.verticalFieldOfViewRadians <= 0.0f ||
                settings.verticalFieldOfViewRadians >= std::numbers::pi_v<float> ||
                settings.aspectRatio <= 0.0f ||
                settings.padding < 1.0f ||
                settings.minimumDistance < 0.0f)
            {
                return std::nullopt;
            }

            const Vec3 target = detail::scale(detail::add(bounds->minimum, bounds->maximum), 0.5f);
            const Vec3 halfExtent = detail::scale(detail::subtract(bounds->maximum, bounds->minimum), 0.5f);
            const float radius = detail::length(halfExtent);
            const float verticalHalfFov = settings.verticalFieldOfViewRadians * 0.5f;
            const float horizontalHalfFov = std::atan(
                std::tan(verticalHalfFov) * settings.aspectRatio);
            const float limitingHalfFov = (std::min)(verticalHalfFov, horizontalHalfFov);
            const float fitDistance = radius > 0.0f
                ? radius / std::sin(limitingHalfFov)
                : 0.0f;
            const float suggestedDistance = (std::max)(
                settings.minimumDistance,
                fitDistance * settings.padding);
            if (!detail::finite(target) || !std::isfinite(radius) ||
                !std::isfinite(suggestedDistance))
            {
                return std::nullopt;
            }
            return FocusTarget{
                *bounds,
                target,
                radius,
                suggestedDistance,
                objects.size()
            };
        }

        [[nodiscard]] std::span<const SceneInteractionRecord> records() const noexcept
        {
            return records_;
        }

        [[nodiscard]] std::uint64_t source_revision() const noexcept
        {
            return sourceRevision_;
        }

        [[nodiscard]] std::uint64_t interaction_revision() const noexcept
        {
            return revision_;
        }

    private:
        std::unique_ptr<epochengine::ray::RayScene> rayScene_{};
        std::vector<SceneInteractionRecord> records_{};
        std::unordered_map<SceneObjectId, std::size_t, SceneObjectIdHash> lookup_{};
        std::uint64_t sourceRevision_{};
        std::uint64_t revision_{};
    };

    struct SceneInteractionContractReport final
    {
        bool persistentIdAuthoritative{};
        bool reorderStable{};
        bool hitResolvedByPersistentId{};
        bool duplicateIdsRejected{};
        bool sourceRevisionPreserved{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return persistentIdAuthoritative &&
                reorderStable &&
                hitResolvedByPersistentId &&
                duplicateIdsRejected &&
                sourceRevisionPreserved;
        }
    };

    [[nodiscard]] inline SceneInteractionContractReport
        run_scene_interaction_contract_checks()
    {
        epochengine::scene::SceneSnapshot snapshot{};
        snapshot.scene_id = "scene.interaction.contract";
        snapshot.revision = 41u;
        snapshot.objects = {
            epochengine::scene::SceneObjectSnapshot{
                .id = 101u,
                .name = "Left",
                .type = "Mesh",
                .position = {-2.0f, 0.0f, 0.0f}
            },
            epochengine::scene::SceneObjectSnapshot{
                .id = 202u,
                .name = "Right",
                .type = "Mesh",
                .position = {2.0f, 0.0f, 0.0f}
            }
        };

        SceneInteractionContractReport result{};
        const auto authoritative = derive_scene_object_id(snapshot, snapshot.objects.front());
        result.persistentIdAuthoritative = authoritative.has_value() &&
            *authoritative == SceneObjectId{101u, 1u};

        SceneInteractionQuery query{};
        const SceneInteractionBuildReport firstBuild = query.rebuild(snapshot);
        const SceneInteractionRecord* firstLeft = query.resolve({101u, 1u});
        const SceneInteractionRecord* firstRight = query.resolve({202u, 1u});
        const bool firstResolved = firstBuild.committed() &&
            firstBuild.committedRevision == 41u &&
            firstLeft != nullptr && firstLeft->snapshotIndex == 0u &&
            firstRight != nullptr && firstRight->snapshotIndex == 1u;

        std::swap(snapshot.objects[0], snapshot.objects[1]);
        snapshot.revision = 42u;
        const SceneInteractionBuildReport reorderedBuild = query.rebuild(snapshot);
        const SceneInteractionRecord* reorderedLeft = query.resolve({101u, 1u});
        const SceneInteractionRecord* reorderedRight = query.resolve({202u, 1u});
        result.reorderStable = firstResolved &&
            reorderedBuild.committed() &&
            reorderedLeft != nullptr && reorderedLeft->snapshotIndex == 1u &&
            reorderedRight != nullptr && reorderedRight->snapshotIndex == 0u;

        epochengine::ray::RayDesc selectionRay{};
        selectionRay.origin = {-2.0f, 0.0f, -5.0f};
        selectionRay.direction = {0.0f, 0.0f, 1.0f};
        const SceneSelectionResult selection = query.trace(selectionRay);
        result.hitResolvedByPersistentId = selection.has_hit() &&
            selection.hit.object == SceneObjectId{101u, 1u} &&
            selection.hit.snapshotIndex == 1u;

        epochengine::scene::SceneSnapshot duplicate = snapshot;
        duplicate.revision = 43u;
        duplicate.objects[1].id = duplicate.objects[0].id;
        const std::uint64_t retainedInteractionRevision = query.interaction_revision();
        const SceneInteractionBuildReport duplicateBuild = query.rebuild(duplicate);
        result.duplicateIdsRejected =
            duplicateBuild.status == SceneInteractionBuildStatus::DuplicateIdentity &&
            duplicateBuild.committedRevision == 42u &&
            duplicateBuild.interactionRevision == retainedInteractionRevision &&
            query.interaction_revision() == retainedInteractionRevision &&
            query.source_revision() == 42u &&
            query.resolve({101u, 1u}) != nullptr &&
            query.resolve({202u, 1u}) != nullptr;
        result.sourceRevisionPreserved =
            firstBuild.sourceRevision == 41u &&
            reorderedBuild.sourceRevision == 42u &&
            reorderedBuild.committedRevision == 42u &&
            selection.sceneRevision == 42u &&
            duplicateBuild.sourceRevision == 43u &&
            query.source_revision() == 42u;
        return result;
    }

    struct ScreenViewport final
    {
        float x{};
        float y{};
        float width{};
        float height{};
    };

    struct ScreenPoint final
    {
        float x{};
        float y{};
    };

    struct CameraFrame final
    {
        Vec3 position{};
        Vec3 forward{0.0f, 0.0f, -1.0f};
        Vec3 up{0.0f, 1.0f, 0.0f};
    };

    struct PerspectiveProjection final
    {
        float verticalFieldOfViewRadians{std::numbers::pi_v<float> / 3.0f};
        float nearDistance{0.01f};
        float farDistance{10'000.0f};
    };

    struct OrthographicProjection final
    {
        float verticalSize{10.0f};
        float nearDistance{};
        float farDistance{10'000.0f};
    };

    namespace detail
    {
        struct CameraBasis final
        {
            Vec3 forward{};
            Vec3 right{};
            Vec3 up{};
        };

        [[nodiscard]] inline std::optional<CameraBasis> camera_basis(
            const CameraFrame& camera) noexcept
        {
            if (!detail::finite(camera.position))
            {
                return std::nullopt;
            }
            const auto forward = detail::normalized(camera.forward);
            if (!forward.has_value())
            {
                return std::nullopt;
            }
            const auto right = detail::normalized(detail::cross(*forward, camera.up));
            if (!right.has_value())
            {
                return std::nullopt;
            }
            const auto up = detail::normalized(detail::cross(*right, *forward));
            if (!up.has_value())
            {
                return std::nullopt;
            }
            return CameraBasis{*forward, *right, *up};
        }

        [[nodiscard]] inline std::optional<std::array<float, 2>> normalized_screen(
            ScreenPoint point,
            ScreenViewport viewport) noexcept
        {
            if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
                !std::isfinite(viewport.x) || !std::isfinite(viewport.y) ||
                !std::isfinite(viewport.width) || !std::isfinite(viewport.height) ||
                viewport.width <= 0.0f || viewport.height <= 0.0f)
            {
                return std::nullopt;
            }
            const float x = ((point.x - viewport.x) / viewport.width) * 2.0f - 1.0f;
            const float y = 1.0f - ((point.y - viewport.y) / viewport.height) * 2.0f;
            return std::array<float, 2>{x, y};
        }

        [[nodiscard]] inline bool valid_distances(float nearDistance, float farDistance) noexcept
        {
            return std::isfinite(nearDistance) && std::isfinite(farDistance) &&
                nearDistance >= 0.0f && farDistance > nearDistance;
        }
    }

    [[nodiscard]] inline std::optional<epochengine::ray::RayDesc> perspective_screen_ray(
        ScreenPoint point,
        ScreenViewport viewport,
        const CameraFrame& camera,
        PerspectiveProjection projection = {},
        std::uint32_t visibilityMask = kAllVisibility) noexcept
    {
        const auto screen = detail::normalized_screen(point, viewport);
        const auto basis = detail::camera_basis(camera);
        if (!screen.has_value() || !basis.has_value() || visibilityMask == 0 ||
            !detail::valid_distances(projection.nearDistance, projection.farDistance) ||
            !std::isfinite(projection.verticalFieldOfViewRadians) ||
            projection.verticalFieldOfViewRadians <= 0.0f ||
            projection.verticalFieldOfViewRadians >= std::numbers::pi_v<float>)
        {
            return std::nullopt;
        }

        const float aspectRatio = viewport.width / viewport.height;
        const float halfHeight = std::tan(projection.verticalFieldOfViewRadians * 0.5f);
        const float halfWidth = halfHeight * aspectRatio;
        const Vec3 direction = detail::add(
            basis->forward,
            detail::add(
                detail::scale(basis->right, (*screen)[0] * halfWidth),
                detail::scale(basis->up, (*screen)[1] * halfHeight)));
        const auto normalizedDirection = detail::normalized(direction);
        if (!normalizedDirection.has_value())
        {
            return std::nullopt;
        }

        epochengine::ray::RayDesc result{
            camera.position,
            *normalizedDirection,
            projection.nearDistance,
            projection.farDistance,
            visibilityMask
        };
        return epochengine::ray::validate(result).valid
            ? std::optional<epochengine::ray::RayDesc>{result}
            : std::nullopt;
    }

    [[nodiscard]] inline std::optional<epochengine::ray::RayDesc> orthographic_screen_ray(
        ScreenPoint point,
        ScreenViewport viewport,
        const CameraFrame& camera,
        OrthographicProjection projection = {},
        std::uint32_t visibilityMask = kAllVisibility) noexcept
    {
        const auto screen = detail::normalized_screen(point, viewport);
        const auto basis = detail::camera_basis(camera);
        if (!screen.has_value() || !basis.has_value() || visibilityMask == 0 ||
            !detail::valid_distances(projection.nearDistance, projection.farDistance) ||
            !std::isfinite(projection.verticalSize) || projection.verticalSize <= 0.0f)
        {
            return std::nullopt;
        }

        const float halfHeight = projection.verticalSize * 0.5f;
        const float halfWidth = halfHeight * (viewport.width / viewport.height);
        const Vec3 origin = detail::add(
            camera.position,
            detail::add(
                detail::scale(basis->right, (*screen)[0] * halfWidth),
                detail::scale(basis->up, (*screen)[1] * halfHeight)));
        epochengine::ray::RayDesc result{
            origin,
            basis->forward,
            projection.nearDistance,
            projection.farDistance,
            visibilityMask
        };
        return epochengine::ray::validate(result).valid
            ? std::optional<epochengine::ray::RayDesc>{result}
            : std::nullopt;
    }

    struct DragPlane final
    {
        Vec3 point{};
        Vec3 normal{0.0f, 1.0f, 0.0f};
    };

    struct DragPlaneHit final
    {
        float distance{};
        Vec3 position{};
    };

    struct DragDelta final
    {
        Vec3 start{};
        Vec3 current{};
        Vec3 delta{};
    };

    [[nodiscard]] inline std::optional<DragPlane> camera_facing_drag_plane(
        Vec3 anchor,
        const CameraFrame& camera) noexcept
    {
        const auto basis = detail::camera_basis(camera);
        if (!detail::finite(anchor) || !basis.has_value())
        {
            return std::nullopt;
        }
        return DragPlane{anchor, basis->forward};
    }

    [[nodiscard]] inline std::optional<DragPlane> axis_drag_plane(
        Vec3 anchor,
        Vec3 axis,
        const CameraFrame& camera) noexcept
    {
        const auto basis = detail::camera_basis(camera);
        const auto normalizedAxis = detail::normalized(axis);
        if (!detail::finite(anchor) || !basis.has_value() || !normalizedAxis.has_value())
        {
            return std::nullopt;
        }
        const Vec3 normalCandidate = detail::subtract(
            basis->forward,
            detail::scale(*normalizedAxis, detail::dot(basis->forward, *normalizedAxis)));
        const auto normal = detail::normalized(normalCandidate);
        if (!normal.has_value())
        {
            return std::nullopt;
        }
        return DragPlane{anchor, *normal};
    }

    [[nodiscard]] inline std::optional<DragPlaneHit> intersect_drag_plane(
        const epochengine::ray::RayDesc& sourceRay,
        DragPlane plane) noexcept
    {
        if (!epochengine::ray::validate(sourceRay).valid || !detail::finite(plane.point))
        {
            return std::nullopt;
        }
        const auto normal = detail::normalized(plane.normal);
        if (!normal.has_value())
        {
            return std::nullopt;
        }

        const epochengine::ray::RayDesc ray = epochengine::ray::normalized(sourceRay);
        const float denominator = detail::dot(ray.direction, *normal);
        if (!std::isfinite(denominator) || std::abs(denominator) <= 1.0e-7f)
        {
            return std::nullopt;
        }
        const float distance = detail::dot(
            detail::subtract(plane.point, ray.origin),
            *normal) / denominator;
        if (!std::isfinite(distance) || distance < ray.minimumDistance ||
            distance > ray.maximumDistance)
        {
            return std::nullopt;
        }
        const Vec3 position = detail::add(ray.origin, detail::scale(ray.direction, distance));
        if (!detail::finite(position))
        {
            return std::nullopt;
        }
        return DragPlaneHit{distance, position};
    }

    [[nodiscard]] inline std::optional<DragDelta> drag_delta(
        const epochengine::ray::RayDesc& startRay,
        const epochengine::ray::RayDesc& currentRay,
        DragPlane plane) noexcept
    {
        const auto start = intersect_drag_plane(startRay, plane);
        const auto current = intersect_drag_plane(currentRay, plane);
        if (!start.has_value() || !current.has_value())
        {
            return std::nullopt;
        }
        return DragDelta{
            start->position,
            current->position,
            detail::subtract(current->position, start->position)
        };
    }
}









