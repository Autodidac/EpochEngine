// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>

export module voxel.sdf;

import voxel.field;

export namespace epochengine::voxel::sdf
{
    // This is separate from VoxelCell::density, whose sign convention differs.
    // Positions, radii, half extents and returned distances are all in meters.
    // There is no implicit density conversion, sampled-grid adapter or tracer.
    enum class Region : std::uint8_t { invalid, solid, surface, outside };
    enum class DistanceQuality : std::uint8_t
    {
        invalid,
        analytic_exact,
        // Hard CSG is sign-correct but not generally an exact Euclidean SDF.
        // These min/max compositions retain the primitives' distance bound.
        conservative_csg_bound
    };

    struct Sphere final
    {
        Float3 center_meters{};
        float radius_meters{};
        MaterialId material{}; // Zero is a valid existing material identity.
    };

    struct Box final
    {
        Float3 center_meters{};
        Float3 half_extent_meters{}; // Axis aligned; strictly positive per axis.
        MaterialId material{};
    };

    class Sample;
    [[nodiscard]] Sample sample(const Sphere&, Float3 point_meters) noexcept;
    [[nodiscard]] Sample sample(const Box&, Float3 point_meters) noexcept;
    [[nodiscard]] Sample hard_union(const Sample&, const Sample&) noexcept;
    [[nodiscard]] Sample hard_intersection(const Sample&, const Sample&) noexcept;
    [[nodiscard]] Sample hard_subtract(
        const Sample& solid, const Sample& cutter, MaterialId cut_material) noexcept;

    // Only validated primitives/CSG can construct a distance in this API.
    class SignedDistanceMeters final
    {
    public:
        [[nodiscard]] constexpr double meters() const noexcept { return meters_; }

    private:
        explicit constexpr SignedDistanceMeters(double meters) noexcept : meters_(meters) {}
        double meters_{};
        friend class Sample;
    };

    class Sample final
    {
    public:
        constexpr Sample() noexcept = default; // Explicitly invalid, not empty space.

        [[nodiscard]] constexpr bool valid() const noexcept { return distance_.has_value(); }
        [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }
        [[nodiscard]] constexpr const std::optional<SignedDistanceMeters>& distance() const noexcept
        {
            return distance_;
        }
        // The limiting primitive/surface owns this label, even outside the solid.
        // Read region() separately; material zero does not mean empty space.
        [[nodiscard]] constexpr MaterialId material() const noexcept { return material_; }
        [[nodiscard]] constexpr DistanceQuality quality() const noexcept { return quality_; }
        [[nodiscard]] constexpr Region region() const noexcept
        {
            if (!distance_) return Region::invalid;
            const double value = distance_->meters();
            return value < 0.0 ? Region::solid : value > 0.0 ? Region::outside : Region::surface;
        }

    private:
        Sample(double meters, MaterialId material, DistanceQuality quality) noexcept
            : distance_(SignedDistanceMeters{meters}), material_(material), quality_(quality) {}

        std::optional<SignedDistanceMeters> distance_{};
        MaterialId material_{};
        DistanceQuality quality_{DistanceQuality::invalid};
        friend Sample sample(const Sphere&, Float3) noexcept;
        friend Sample sample(const Box&, Float3) noexcept;
        friend Sample hard_union(const Sample&, const Sample&) noexcept;
        friend Sample hard_intersection(const Sample&, const Sample&) noexcept;
        friend Sample hard_subtract(const Sample&, const Sample&, MaterialId) noexcept;
    };

    [[nodiscard]] inline bool finite(Float3 value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    [[nodiscard]] inline bool valid(const Sphere& sphere) noexcept
    {
        return finite(sphere.center_meters) && std::isfinite(sphere.radius_meters)
            && sphere.radius_meters > 0.0f;
    }

    [[nodiscard]] inline bool valid(const Box& box) noexcept
    {
        return finite(box.center_meters) && finite(box.half_extent_meters)
            && box.half_extent_meters.x > 0.0f && box.half_extent_meters.y > 0.0f
            && box.half_extent_meters.z > 0.0f;
    }

    [[nodiscard]] inline Sample sample(const Sphere& sphere, Float3 point_meters) noexcept
    {
        if (!valid(sphere) || !finite(point_meters)) return {};
        // Promote before subtraction: finite float coordinates must not overflow
        // float intermediates. hypot also avoids avoidable squared-length overflow.
        const double dx = static_cast<double>(point_meters.x) - sphere.center_meters.x;
        const double dy = static_cast<double>(point_meters.y) - sphere.center_meters.y;
        const double dz = static_cast<double>(point_meters.z) - sphere.center_meters.z;
        const double distance = std::hypot(dx, dy, dz) - sphere.radius_meters;
        if (!std::isfinite(distance)) return {};
        return {distance, sphere.material, DistanceQuality::analytic_exact};
    }

    [[nodiscard]] inline Sample sample(const Box& box, Float3 point_meters) noexcept
    {
        if (!valid(box) || !finite(point_meters)) return {};
        const double x = std::abs(static_cast<double>(point_meters.x) - box.center_meters.x)
            - box.half_extent_meters.x;
        const double y = std::abs(static_cast<double>(point_meters.y) - box.center_meters.y)
            - box.half_extent_meters.y;
        const double z = std::abs(static_cast<double>(point_meters.z) - box.center_meters.z)
            - box.half_extent_meters.z;
        const double outside = std::hypot((std::max)(x, 0.0), (std::max)(y, 0.0), (std::max)(z, 0.0));
        const double inside = (std::min)((std::max)({x, y, z}), 0.0);
        const double distance = outside + inside;
        if (!std::isfinite(distance)) return {};
        return {distance, box.material, DistanceQuality::analytic_exact};
    }

    // Equal values retain the left surface owner. No approximate equality or
    // iteration-order-dependent material blending is hidden in these operators.
    [[nodiscard]] inline Sample hard_union(const Sample& left, const Sample& right) noexcept
    {
        if (!left || !right) return {};
        const Sample& chosen = left.distance()->meters() <= right.distance()->meters() ? left : right;
        return {chosen.distance()->meters(), chosen.material(), DistanceQuality::conservative_csg_bound};
    }

    [[nodiscard]] inline Sample hard_intersection(const Sample& left, const Sample& right) noexcept
    {
        if (!left || !right) return {};
        const Sample& chosen = left.distance()->meters() >= right.distance()->meters() ? left : right;
        return {chosen.distance()->meters(), chosen.material(), DistanceQuality::conservative_csg_bound};
    }

    [[nodiscard]] inline Sample hard_subtract(
        const Sample& solid, const Sample& cutter, MaterialId cut_material) noexcept
    {
        if (!solid || !cutter) return {};
        const double retained_distance = solid.distance()->meters();
        const double cut_distance = -cutter.distance()->meters();
        if (retained_distance >= cut_distance)
            return {retained_distance, solid.material(), DistanceQuality::conservative_csg_bound};
        // The caller owns the exposed cut surface, not the discarded cutter's
        // material. This label also identifies that surface from inside its void.
        return {cut_distance, cut_material, DistanceQuality::conservative_csg_bound};
    }

    using Primitive = std::variant<Sphere, Box>;

    [[nodiscard]] inline bool valid(const Primitive& primitive) noexcept
    {
        if (const auto* sphere = std::get_if<Sphere>(&primitive)) return valid(*sphere);
        if (const auto* box = std::get_if<Box>(&primitive)) return valid(*box);
        return false;
    }

    [[nodiscard]] inline Sample sample_primitive(const Primitive& primitive, Float3 point_meters) noexcept
    {
        if (const auto* sphere = std::get_if<Sphere>(&primitive)) return sample(*sphere, point_meters);
        if (const auto* box = std::get_if<Box>(&primitive)) return sample(*box, point_meters);
        return {};
    }

    enum class EditKind : std::uint8_t { deposit, intersect, subtract };
    struct Edit final
    {
        EditKind kind{EditKind::deposit};
        Primitive brush{};
        MaterialId cut_material{}; // Used only by subtract; zero is allowed.
    };
    enum class AppendStatus : std::uint8_t { applied, invalid_base, invalid_edit, capacity_exceeded };

    // A bounded local edit record and sampler, not a world store, mesh generator,
    // streaming system or accelerated query structure. Ordered CSG is deliberate:
    // deposit after carving can refill a cavity; carving afterward removes it.
    class EditVolume final
    {
    public:
        static constexpr std::size_t maximum_edits = 64u;
        explicit EditVolume(Primitive base) noexcept : base_(base), valid_(sdf::valid(base)) {}

        [[nodiscard]] bool valid() const noexcept { return valid_; }
        [[nodiscard]] std::size_t size() const noexcept { return count_; }

        [[nodiscard]] AppendStatus append(const Edit& edit) noexcept
        {
            if (!valid_) return AppendStatus::invalid_base;
            if (!sdf::valid(edit.brush) || (edit.kind != EditKind::deposit
                && edit.kind != EditKind::intersect && edit.kind != EditKind::subtract))
                return AppendStatus::invalid_edit;
            if (count_ == maximum_edits) return AppendStatus::capacity_exceeded;
            edits_[count_] = edit;
            ++count_;
            return AppendStatus::applied;
        }

        [[nodiscard]] Sample sample(Float3 point_meters) const noexcept
        {
            if (!valid_ || !finite(point_meters)) return {};
            Sample result = sample_primitive(base_, point_meters);
            for (std::size_t index = 0; index < count_; ++index)
            {
                const auto& edit = edits_[index];
                const Sample brush = sample_primitive(edit.brush, point_meters);
                switch (edit.kind)
                {
                case EditKind::deposit: result = hard_union(result, brush); break;
                case EditKind::intersect: result = hard_intersection(result, brush); break;
                case EditKind::subtract: result = hard_subtract(result, brush, edit.cut_material); break;
                }
            }
            return result;
        }

    private:
        Primitive base_{};
        std::array<Edit, maximum_edits> edits_{};
        std::size_t count_{};
        bool valid_{};
    };
}
