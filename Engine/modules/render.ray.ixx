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
#include <atomic>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

export module render.ray;

import render.math;

export namespace epochengine::ray
{
    using Vec3 = epochengine::render_math::Float3;

    struct RayDesc final
    {
        Vec3 origin{};
        Vec3 direction{0.0f, 0.0f, 1.0f};
        float minimumDistance{0.001f};
        float maximumDistance{(std::numeric_limits<float>::max)()};
        std::uint32_t visibilityMask{(std::numeric_limits<std::uint32_t>::max)()};
    };

    struct RayValidation final
    {
        bool valid{};
        bool finite{};
        bool nonzeroDirection{};
        bool orderedDistance{};
    };

    [[nodiscard]] inline RayValidation validate(const RayDesc& ray) noexcept
    {
        const bool finite =
            std::isfinite(ray.origin.x) &&
            std::isfinite(ray.origin.y) &&
            std::isfinite(ray.origin.z) &&
            std::isfinite(ray.direction.x) &&
            std::isfinite(ray.direction.y) &&
            std::isfinite(ray.direction.z) &&
            std::isfinite(ray.minimumDistance) &&
            std::isfinite(ray.maximumDistance);
        const bool nonzeroDirection = epochengine::render_math::dot(ray.direction, ray.direction) > 1.0e-12f;
        const bool orderedDistance =
            ray.minimumDistance >= 0.0f &&
            ray.maximumDistance >= ray.minimumDistance;
        return {
            finite && nonzeroDirection && orderedDistance && ray.visibilityMask != 0,
            finite,
            nonzeroDirection,
            orderedDistance
        };
    }

    [[nodiscard]] inline RayDesc normalized(RayDesc ray) noexcept
    {
        ray.direction = epochengine::render_math::normalize(ray.direction);
        ray.minimumDistance = (std::max)(0.0f, ray.minimumDistance);
        ray.maximumDistance = (std::max)(ray.minimumDistance, ray.maximumDistance);
        return ray;
    }

    struct Aabb final
    {
        Vec3 minimum{-0.5f, -0.5f, -0.5f};
        Vec3 maximum{0.5f, 0.5f, 0.5f};
    };

    struct Sphere final
    {
        Vec3 center{};
        float radius{0.5f};
    };

    struct Triangle final
    {
        Vec3 a{};
        Vec3 b{1.0f, 0.0f, 0.0f};
        Vec3 c{0.0f, 1.0f, 0.0f};
        bool doubleSided{true};
    };

    enum class PrimitiveKind : std::uint8_t
    {
        Aabb,
        Sphere,
        Triangle
    };

    struct PrimitiveHandle final
    {
        std::uint32_t index{(std::numeric_limits<std::uint32_t>::max)()};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return index != (std::numeric_limits<std::uint32_t>::max)() && generation != 0;
        }

        friend constexpr bool operator==(const PrimitiveHandle&, const PrimitiveHandle&) noexcept = default;
    };

    struct QueryObjectId final
    {
        std::uint64_t value{};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return value != 0 && generation != 0;
        }

        friend constexpr bool operator==(const QueryObjectId&, const QueryObjectId&) noexcept = default;
    };

    struct PrimitiveDesc final
    {
        PrimitiveKind kind{PrimitiveKind::Aabb};
        Aabb aabb{};
        Sphere sphere{};
        Triangle triangle{};
        std::uint32_t visibilityMask{(std::numeric_limits<std::uint32_t>::max)()};
        QueryObjectId object{};
        std::uint32_t primitiveIndex{};
        std::uint32_t materialId{};
        bool enabled{true};
    };

    struct PrimitiveIntersection final
    {
        float distance{};
        Vec3 geometricNormal{};
        std::array<float, 2> barycentrics{};
    };

    [[nodiscard]] inline bool validate(const Aabb& bounds) noexcept
    {
        return epochengine::render_math::finite(bounds.minimum) && epochengine::render_math::finite(bounds.maximum) &&
               bounds.minimum.x <= bounds.maximum.x &&
               bounds.minimum.y <= bounds.maximum.y &&
               bounds.minimum.z <= bounds.maximum.z;
    }

    [[nodiscard]] inline bool validate(const Sphere& sphere) noexcept
    {
        return epochengine::render_math::finite(sphere.center) && std::isfinite(sphere.radius) && sphere.radius > 0.0f;
    }

    [[nodiscard]] inline bool validate(const Triangle& triangle) noexcept
    {
        if (!epochengine::render_math::finite(triangle.a) || !epochengine::render_math::finite(triangle.b) || !epochengine::render_math::finite(triangle.c))
        {
            return false;
        }
        const Vec3 normal = epochengine::render_math::cross(epochengine::render_math::subtract(triangle.b, triangle.a), epochengine::render_math::subtract(triangle.c, triangle.a));
        return epochengine::render_math::dot(normal, normal) > 1.0e-12f;
    }

    [[nodiscard]] inline bool validate(const PrimitiveDesc& primitive) noexcept
    {
        if (primitive.visibilityMask == 0)
        {
            return false;
        }
        switch (primitive.kind)
        {
        case PrimitiveKind::Aabb: return validate(primitive.aabb);
        case PrimitiveKind::Sphere: return validate(primitive.sphere);
        case PrimitiveKind::Triangle: return validate(primitive.triangle);
        }
        return false;
    }

    [[nodiscard]] inline std::optional<PrimitiveIntersection> intersect(
        const RayDesc& sourceRay,
        const Aabb& bounds) noexcept
    {
        if (!validate(sourceRay).valid || !validate(bounds))
        {
            return std::nullopt;
        }

        const RayDesc ray = normalized(sourceRay);
        float entry = -(std::numeric_limits<float>::max)();
        float exit = (std::numeric_limits<float>::max)();
        Vec3 entryNormal{};
        Vec3 exitNormal{};

        const auto testAxis = [&entry, &exit, &entryNormal, &exitNormal](
            float origin,
            float direction,
            float minimum,
            float maximum,
            Vec3 negativeNormal,
            Vec3 positiveNormal) noexcept -> bool
        {
            if (std::abs(direction) <= 1.0e-8f)
            {
                return origin >= minimum && origin <= maximum;
            }

            float nearDistance = (minimum - origin) / direction;
            float farDistance = (maximum - origin) / direction;
            Vec3 nearNormal = negativeNormal;
            Vec3 farNormal = positiveNormal;
            if (nearDistance > farDistance)
            {
                std::swap(nearDistance, farDistance);
                std::swap(nearNormal, farNormal);
            }
            if (nearDistance > entry)
            {
                entry = nearDistance;
                entryNormal = nearNormal;
            }
            if (farDistance < exit)
            {
                exit = farDistance;
                exitNormal = farNormal;
            }
            return entry <= exit;
        };

        if (!testAxis(ray.origin.x, ray.direction.x, bounds.minimum.x, bounds.maximum.x,
                      {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}) ||
            !testAxis(ray.origin.y, ray.direction.y, bounds.minimum.y, bounds.maximum.y,
                      {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}) ||
            !testAxis(ray.origin.z, ray.direction.z, bounds.minimum.z, bounds.maximum.z,
                      {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}))
        {
            return std::nullopt;
        }

        const bool entryOnSegment = entry >= ray.minimumDistance;
        const float distance = entryOnSegment ? entry : exit;
        if (distance < ray.minimumDistance || distance > ray.maximumDistance)
        {
            return std::nullopt;
        }
        return PrimitiveIntersection{
            distance,
            entryOnSegment ? entryNormal : exitNormal,
            {}
        };
    }
    [[nodiscard]] inline std::optional<PrimitiveIntersection> intersect(
        const RayDesc& sourceRay,
        const Sphere& sphere) noexcept
    {
        if (!validate(sourceRay).valid || !validate(sphere))
        {
            return std::nullopt;
        }

        const RayDesc ray = normalized(sourceRay);
        const Vec3 offset = epochengine::render_math::subtract(ray.origin, sphere.center);
        const float halfB = epochengine::render_math::dot(offset, ray.direction);
        const float c = epochengine::render_math::dot(offset, offset) - sphere.radius * sphere.radius;
        const float discriminant = halfB * halfB - c;
        if (discriminant < 0.0f)
        {
            return std::nullopt;
        }

        const float root = std::sqrt(discriminant);
        float distance = -halfB - root;
        if (distance < ray.minimumDistance || distance > ray.maximumDistance)
        {
            distance = -halfB + root;
        }
        if (distance < ray.minimumDistance || distance > ray.maximumDistance)
        {
            return std::nullopt;
        }

        const Vec3 position = epochengine::render_math::point_at(ray.origin, ray.direction, distance);
        return PrimitiveIntersection{
            distance,
            epochengine::render_math::normalize(epochengine::render_math::subtract(position, sphere.center)),
            {}
        };
    }

    [[nodiscard]] inline std::optional<PrimitiveIntersection> intersect(
        const RayDesc& sourceRay,
        const Triangle& triangle) noexcept
    {
        if (!validate(sourceRay).valid || !validate(triangle))
        {
            return std::nullopt;
        }

        const RayDesc ray = normalized(sourceRay);
        const Vec3 edgeA = epochengine::render_math::subtract(triangle.b, triangle.a);
        const Vec3 edgeB = epochengine::render_math::subtract(triangle.c, triangle.a);
        const Vec3 p = epochengine::render_math::cross(ray.direction, edgeB);
        const float determinant = epochengine::render_math::dot(edgeA, p);
        if (triangle.doubleSided)
        {
            if (std::abs(determinant) <= 1.0e-8f)
            {
                return std::nullopt;
            }
        }
        else if (determinant <= 1.0e-8f)
        {
            return std::nullopt;
        }

        const float inverseDeterminant = 1.0f / determinant;
        const Vec3 translated = epochengine::render_math::subtract(ray.origin, triangle.a);
        const float u = epochengine::render_math::dot(translated, p) * inverseDeterminant;
        if (u < 0.0f || u > 1.0f)
        {
            return std::nullopt;
        }

        const Vec3 q = epochengine::render_math::cross(translated, edgeA);
        const float v = epochengine::render_math::dot(ray.direction, q) * inverseDeterminant;
        if (v < 0.0f || u + v > 1.0f)
        {
            return std::nullopt;
        }

        const float distance = epochengine::render_math::dot(edgeB, q) * inverseDeterminant;
        if (distance < ray.minimumDistance || distance > ray.maximumDistance)
        {
            return std::nullopt;
        }

        const Vec3 normal = epochengine::render_math::normalize(
            epochengine::render_math::cross(edgeA, edgeB));
        return PrimitiveIntersection{distance, normal, {u, v}};
    }

    [[nodiscard]] inline std::optional<PrimitiveIntersection> intersect(
        const RayDesc& ray,
        const PrimitiveDesc& primitive) noexcept
    {
        switch (primitive.kind)
        {
        case PrimitiveKind::Aabb: return intersect(ray, primitive.aabb);
        case PrimitiveKind::Sphere: return intersect(ray, primitive.sphere);
        case PrimitiveKind::Triangle: return intersect(ray, primitive.triangle);
        }
        return std::nullopt;
    }

    enum class QueryMode : std::uint8_t
    {
        Closest,
        Any
    };

    enum class QueryStatus : std::uint8_t
    {
        Invalid,
        Miss,
        Hit
    };

    struct RayHit final
    {
        PrimitiveHandle primitive{};
        PrimitiveKind kind{PrimitiveKind::Aabb};
        float distance{};
        Vec3 position{};
        Vec3 geometricNormal{};
        std::array<float, 2> barycentrics{};
        QueryObjectId object{};
        std::uint32_t primitiveIndex{};
        std::uint32_t materialId{};
        bool frontFace{};
    };

    struct RayQueryResult final
    {
        QueryStatus status{QueryStatus::Invalid};
        RayHit hit{};
        std::uint32_t testedPrimitives{};
    };

    struct RaySceneMetrics final
    {
        std::size_t slotCount{};
        std::size_t activePrimitiveCount{};
        std::size_t enabledPrimitiveCount{};
        std::size_t aabbCount{};
        std::size_t sphereCount{};
        std::size_t triangleCount{};
        std::uint64_t revision{};
        std::uint64_t queryCount{};
        std::uint64_t primitiveTestCount{};
    };

    class RayScene final
    {
    public:
        [[nodiscard]] PrimitiveHandle create(PrimitiveDesc desc)
        {
            if (!validate(desc))
            {
                return {};
            }

            std::uint32_t index{};
            if (!freeSlots_.empty())
            {
                index = freeSlots_.back();
                freeSlots_.pop_back();
            }
            else
            {
                index = static_cast<std::uint32_t>(slots_.size());
                slots_.push_back({});
            }

            Slot& slot = slots_[index];
            if (slot.generation == 0)
            {
                slot.generation = 1;
            }
            slot.desc = desc;
            slot.occupied = true;
            ++revision_;
            return {index, slot.generation};
        }

        [[nodiscard]] bool destroy(PrimitiveHandle handle) noexcept
        {
            Slot* slot = resolve(handle);
            if (slot == nullptr)
            {
                return false;
            }
            slot->occupied = false;
            slot->generation = next_generation(slot->generation);
            freeSlots_.push_back(handle.index);
            ++revision_;
            return true;
        }

        [[nodiscard]] bool update(PrimitiveHandle handle, PrimitiveDesc desc) noexcept
        {
            Slot* slot = resolve(handle);
            if (slot == nullptr || !validate(desc))
            {
                return false;
            }
            slot->desc = desc;
            ++revision_;
            return true;
        }

        [[nodiscard]] std::optional<PrimitiveDesc> get(PrimitiveHandle handle) const noexcept
        {
            const Slot* slot = resolve(handle);
            return slot == nullptr ? std::nullopt : std::optional<PrimitiveDesc>{slot->desc};
        }

        [[nodiscard]] RayQueryResult trace(
            RayDesc ray,
            QueryMode mode = QueryMode::Closest) const noexcept
        {
            RayQueryResult result{};
            if (!validate(ray).valid)
            {
                return result;
            }

            result.status = QueryStatus::Miss;
            ray = normalized(ray);
            float closestDistance = ray.maximumDistance;
            queryCount_.fetch_add(1, std::memory_order_relaxed);

            for (std::uint32_t index = 0; index < slots_.size(); ++index)
            {
                const Slot& slot = slots_[index];
                if (!slot.occupied || !slot.desc.enabled ||
                    (slot.desc.visibilityMask & ray.visibilityMask) == 0)
                {
                    continue;
                }

                RayDesc boundedRay = ray;
                boundedRay.maximumDistance = closestDistance;
                ++result.testedPrimitives;
                primitiveTestCount_.fetch_add(1, std::memory_order_relaxed);
                const auto intersection = intersect(boundedRay, slot.desc);
                if (!intersection.has_value())
                {
                    continue;
                }

                result.status = QueryStatus::Hit;
                result.hit = {
                    {index, slot.generation},
                    slot.desc.kind,
                    intersection->distance,
                    epochengine::render_math::point_at(
                        ray.origin,
                        ray.direction,
                        intersection->distance),
                    intersection->geometricNormal,
                    intersection->barycentrics,
                    slot.desc.object,
                    slot.desc.primitiveIndex,
                    slot.desc.materialId,
                    epochengine::render_math::dot(
                        ray.direction,
                        intersection->geometricNormal) < 0.0f
                };
                closestDistance = intersection->distance;
                if (mode == QueryMode::Any)
                {
                    return result;
                }
            }
            return result;
        }

        [[nodiscard]] RaySceneMetrics metrics() const noexcept
        {
            RaySceneMetrics result{};
            result.slotCount = slots_.size();
            result.revision = revision_;
            result.queryCount = queryCount_.load(std::memory_order_relaxed);
            result.primitiveTestCount = primitiveTestCount_.load(std::memory_order_relaxed);
            for (const Slot& slot : slots_)
            {
                if (!slot.occupied)
                {
                    continue;
                }
                ++result.activePrimitiveCount;
                if (slot.desc.enabled)
                {
                    ++result.enabledPrimitiveCount;
                }
                switch (slot.desc.kind)
                {
                case PrimitiveKind::Aabb: ++result.aabbCount; break;
                case PrimitiveKind::Sphere: ++result.sphereCount; break;
                case PrimitiveKind::Triangle: ++result.triangleCount; break;
                }
            }
            return result;
        }

    private:
        struct Slot final
        {
            PrimitiveDesc desc{};
            std::uint32_t generation{1};
            bool occupied{};
        };

        [[nodiscard]] static constexpr std::uint32_t next_generation(std::uint32_t generation) noexcept
        {
            ++generation;
            return generation == 0 ? 1 : generation;
        }

        [[nodiscard]] Slot* resolve(PrimitiveHandle handle) noexcept
        {
            if (!handle.valid() || handle.index >= slots_.size())
            {
                return nullptr;
            }
            Slot& slot = slots_[handle.index];
            return slot.occupied && slot.generation == handle.generation ? &slot : nullptr;
        }

        [[nodiscard]] const Slot* resolve(PrimitiveHandle handle) const noexcept
        {
            if (!handle.valid() || handle.index >= slots_.size())
            {
                return nullptr;
            }
            const Slot& slot = slots_[handle.index];
            return slot.occupied && slot.generation == handle.generation ? &slot : nullptr;
        }

        std::vector<Slot> slots_{};
        std::vector<std::uint32_t> freeSlots_{};
        std::uint64_t revision_{};
        mutable std::atomic<std::uint64_t> queryCount_{};
        mutable std::atomic<std::uint64_t> primitiveTestCount_{};
    };

    struct VoxelCoord final
    {
        std::int32_t x{};
        std::int32_t y{};
        std::int32_t z{};

        friend constexpr bool operator==(const VoxelCoord&, const VoxelCoord&) noexcept = default;
    };

    struct VoxelTraceSettings final
    {
        float cellSize{1.0f};
        std::uint32_t maximumSteps{4096};
    };

    struct VoxelHit final
    {
        bool hit{};
        VoxelCoord cell{};
        float distance{};
        Vec3 position{};
        Vec3 geometricNormal{};
        std::uint32_t steps{};
    };

    template <typename IsOccupied>
    [[nodiscard]] VoxelHit trace_voxels(
        const RayDesc& sourceRay,
        VoxelTraceSettings settings,
        IsOccupied&& isOccupied)
    {
        VoxelHit result{};
        if (!validate(sourceRay).valid || !std::isfinite(settings.cellSize) ||
            settings.cellSize <= 0.0f || settings.maximumSteps == 0)
        {
            return result;
        }

        const RayDesc ray = normalized(sourceRay);
        const Vec3 position = epochengine::render_math::point_at(
            ray.origin,
            ray.direction,
            ray.minimumDistance);
        const auto coordinateFor = [cellSize = static_cast<double>(settings.cellSize)](
            float coordinate) noexcept -> std::optional<std::int32_t>
        {
            const double scaled = std::floor(static_cast<double>(coordinate) / cellSize);
            if (!std::isfinite(scaled) ||
                scaled < static_cast<double>((std::numeric_limits<std::int32_t>::min)()) ||
                scaled > static_cast<double>((std::numeric_limits<std::int32_t>::max)()))
            {
                return std::nullopt;
            }
            return static_cast<std::int32_t>(scaled);
        };

        const auto xCell = coordinateFor(position.x);
        const auto yCell = coordinateFor(position.y);
        const auto zCell = coordinateFor(position.z);
        if (!xCell.has_value() || !yCell.has_value() || !zCell.has_value())
        {
            return result;
        }
        VoxelCoord cell{*xCell, *yCell, *zCell};

        struct Axis final
        {
            std::int32_t step{};
            double nextDistance{};
            double deltaDistance{};
        };
        const auto axisSetup = [cellSize = static_cast<double>(settings.cellSize)](
            float coordinate,
            float direction,
            std::int32_t cellCoordinate) noexcept -> Axis
        {
            if (std::abs(direction) <= 1.0e-8f)
            {
                const double infinity = (std::numeric_limits<double>::max)();
                return {0, infinity, infinity};
            }

            const std::int32_t step = direction > 0.0f ? 1 : -1;
            const std::int64_t boundaryCell =
                static_cast<std::int64_t>(cellCoordinate) + (step > 0 ? 1 : 0);
            const double boundary = static_cast<double>(boundaryCell) * cellSize;
            return {
                step,
                (boundary - static_cast<double>(coordinate)) /
                    static_cast<double>(direction),
                cellSize / std::abs(static_cast<double>(direction))
            };
        };
        const auto canStep = [](std::int32_t coordinate, std::int32_t step) noexcept
        {
            return (step >= 0 || coordinate != (std::numeric_limits<std::int32_t>::min)()) &&
                   (step <= 0 || coordinate != (std::numeric_limits<std::int32_t>::max)());
        };

        Axis xAxis = axisSetup(position.x, ray.direction.x, cell.x);
        Axis yAxis = axisSetup(position.y, ray.direction.y, cell.y);
        Axis zAxis = axisSetup(position.z, ray.direction.z, cell.z);
        double travelled = ray.minimumDistance;
        Vec3 normal{};

        for (std::uint32_t step = 0; step < settings.maximumSteps; ++step)
        {
            result.steps = step + 1;
            if (isOccupied(cell))
            {
                return {
                    true,
                    cell,
                    static_cast<float>(travelled),
                    epochengine::render_math::point_at(
                        ray.origin,
                        ray.direction,
                        static_cast<float>(travelled)),
                    normal,
                    step + 1
                };
            }

            const double nextDistance = (std::min)({
                xAxis.nextDistance,
                yAxis.nextDistance,
                zAxis.nextDistance
            });
            travelled = static_cast<double>(ray.minimumDistance) + nextDistance;
            if (!std::isfinite(travelled) || travelled > ray.maximumDistance)
            {
                break;
            }

            const double tieTolerance = 1.0e-9 * (std::max)(1.0, std::abs(nextDistance));
            const bool stepX = std::abs(xAxis.nextDistance - nextDistance) <= tieTolerance;
            const bool stepY = std::abs(yAxis.nextDistance - nextDistance) <= tieTolerance;
            const bool stepZ = std::abs(zAxis.nextDistance - nextDistance) <= tieTolerance;
            if ((stepX && !canStep(cell.x, xAxis.step)) ||
                (stepY && !canStep(cell.y, yAxis.step)) ||
                (stepZ && !canStep(cell.z, zAxis.step)))
            {
                break;
            }

            Vec3 crossedNormal{};
            if (stepX)
            {
                cell.x += xAxis.step;
                crossedNormal.x = -static_cast<float>(xAxis.step);
                xAxis.nextDistance += xAxis.deltaDistance;
            }
            if (stepY)
            {
                cell.y += yAxis.step;
                crossedNormal.y = -static_cast<float>(yAxis.step);
                yAxis.nextDistance += yAxis.deltaDistance;
            }
            if (stepZ)
            {
                cell.z += zAxis.step;
                crossedNormal.z = -static_cast<float>(zAxis.step);
                zAxis.nextDistance += zAxis.deltaDistance;
            }
            normal = epochengine::render_math::normalize(crossedNormal);
        }

        return result;
    }
}