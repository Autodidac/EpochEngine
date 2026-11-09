// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <array>
#include <cmath>
#include <limits>
#include <type_traits>

import voxel.field;
import voxel.sdf;

namespace epochengine::voxel::sdf::contracts
{
    namespace
    {
        [[nodiscard]] bool near(const Sample& sample, double meters) noexcept
        {
            return sample && std::abs(sample.distance()->meters() - meters) <= 1.0e-9;
        }

        [[nodiscard]] bool same_sample(const Sample& left, const Sample& right) noexcept
        {
            return left && right && left.distance()->meters() == right.distance()->meters()
                && left.material() == right.material() && left.quality() == right.quality();
        }
    }

    // Status codes remain active in Release; no assert(), renderer or I/O needed.
    // 0=pass, 1=units/sign, 2=invalid input, 3=CSG/material, 4=cave,
    // 5=overhang, 6=deposition/order, 7=determinism, 8=capacity/rejection.
    [[nodiscard]] int run_contract() noexcept
    {
        static_assert(!std::is_constructible_v<SignedDistanceMeters, double>);
        static_assert(!std::is_convertible_v<VoxelCell, Sample>);
        static_assert(!std::is_convertible_v<float, Sample>);
        const Sphere sphere{{0.0f, 0.0f, 0.0f}, 2.0f, 7u};
        const Box box{{}, {2.0f, 3.0f, 4.0f}, 11u};
        if (!near(sample(sphere, {}), -2.0) || !near(sample(sphere, {2.0f, 0.0f, 0.0f}), 0.0)
            || !near(sample(sphere, {5.0f, 0.0f, 0.0f}), 3.0)
            || sample(sphere, {}).region() != Region::solid
            || sample(sphere, {2.0f, 0.0f, 0.0f}).region() != Region::surface
            || sample(sphere, {5.0f, 0.0f, 0.0f}).region() != Region::outside
            || sample(sphere, {}).quality() != DistanceQuality::analytic_exact
            || !near(sample(Sphere{{10.0f, -4.0f, 2.0f}, 2.0f, 7u}, {13.0f, -4.0f, 2.0f}), 1.0)
            || !near(sample(Sphere{{}, 20.0f, 7u}, {50.0f, 0.0f, 0.0f}), 30.0)
            || !near(sample(box, {}), -2.0) || !near(sample(box, {2.0f, 0.0f, 0.0f}), 0.0)
            || !near(sample(box, {5.0f, 0.0f, 0.0f}), 3.0)
            || !near(sample(box, {5.0f, 7.0f, 4.0f}), 5.0)
            || classify_density(-1.0f) != OccupancyClass::Empty)
            return 1;

        const float nan = (std::numeric_limits<float>::quiet_NaN)();
        const float infinity = (std::numeric_limits<float>::infinity)();
        for (const Sphere invalid : std::array{Sphere{{}, 0.0f, 1u}, Sphere{{}, -1.0f, 1u},
                Sphere{{}, infinity, 1u}, Sphere{{nan, 0.0f, 0.0f}, 1.0f, 1u}})
            if (valid(invalid) || sample(invalid, {})) return 2;
        for (const Box invalid : std::array{Box{{}, {0.0f, 1.0f, 1.0f}, 1u},
                Box{{}, {1.0f, -1.0f, 1.0f}, 1u}, Box{{}, {1.0f, nan, 1.0f}, 1u},
                Box{{infinity, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 1u}})
            if (valid(invalid) || sample(invalid, {})) return 2;
        if (sample(sphere, {nan, 0.0f, 0.0f}) || sample(box, {0.0f, infinity, 0.0f})
            || Sample{}.region() != Region::invalid || Sample{}.distance().has_value()
            || hard_union(Sample{}, sample(sphere, {}))
            || hard_intersection(sample(box, {}), Sample{})
            || hard_subtract(Sample{}, sample(sphere, {}), 0u)) return 2;
        const float extreme = (std::numeric_limits<float>::max)();
        const auto far = sample(Sphere{{-extreme, 0.0f, 0.0f}, 1.0f, 0u}, {extreme, 0.0f, 0.0f});
        if (!far || !std::isfinite(far.distance()->meters()) || far.material() != 0u) return 2;

        const auto left = sample(sphere, {});
        const auto right = sample(Sphere{{}, 2.0f, 19u}, {});
        const auto tied_union = hard_union(left, right);
        const auto tied_intersection = hard_intersection(left, right);
        const auto clipped = hard_intersection(sample(sphere, {1.5f, 0.0f, 0.0f}),
            sample(Box{{}, {1.0f, 3.0f, 3.0f}, 21u}, {1.5f, 0.0f, 0.0f}));
        if (tied_union.material() != 7u || tied_intersection.material() != 7u
            || tied_union.quality() != DistanceQuality::conservative_csg_bound
            || !near(clipped, 0.5) || clipped.material() != 21u) return 3;
        EditVolume clipped_volume{sphere};
        if (clipped_volume.append({EditKind::intersect, Box{{}, {1.0f, 3.0f, 3.0f}, 21u}, 0u})
                != AppendStatus::applied
            || !same_sample(clipped, clipped_volume.sample({1.5f, 0.0f, 0.0f}))) return 3;

        const Box rock{{}, {4.0f, 4.0f, 4.0f}, 1u};
        const Sphere cavity{{}, 2.0f, 99u};
        EditVolume cave{rock};
        if (cave.append({EditKind::subtract, cavity, 3u}) != AppendStatus::applied) return 4;
        // The same x/z column contains solid, air, solid: not a height surface.
        if (cave.sample({0.0f, -5.0f, 0.0f}).region() != Region::outside
            || cave.sample({0.0f, -3.0f, 0.0f}).region() != Region::solid
            || cave.sample({}).region() != Region::outside
            || cave.sample({0.0f, 3.0f, 0.0f}).region() != Region::solid
            || cave.sample({0.0f, 5.0f, 0.0f}).region() != Region::outside
            || cave.sample({0.0f, 2.0f, 0.0f}).region() != Region::surface
            || cave.sample({0.0f, 2.25f, 0.0f}).material() != 3u
            || cave.sample({0.0f, 3.0f, 0.0f}).material() != 1u) return 4;
        const auto zero_material_cut = hard_subtract(sample(rock, {0.0f, 2.25f, 0.0f}),
            sample(cavity, {0.0f, 2.25f, 0.0f}), 0u);
        if (!zero_material_cut || zero_material_cut.material() != 0u
            || zero_material_cut.region() != Region::solid) return 4;

        EditVolume overhang{Box{{-3.0f, 0.0f, 0.0f}, {1.0f, 4.0f, 2.0f}, 1u}};
        if (overhang.append({EditKind::deposit, Box{{0.0f, 3.0f, 0.0f}, {4.0f, 1.0f, 2.0f}, 2u}, 0u})
                != AppendStatus::applied
            || overhang.sample({2.0f, 0.0f, 0.0f}).region() != Region::outside
            || overhang.sample({2.0f, 3.0f, 0.0f}).region() != Region::solid
            || overhang.sample({2.0f, 3.0f, 0.0f}).material() != 2u
            || overhang.sample({2.0f, 5.0f, 0.0f}).region() != Region::outside
            || overhang.sample({-3.0f, 3.0f, 0.0f}).region() != Region::solid) return 5;

        const Sphere deposit{{}, 0.5f, 5u};
        EditVolume filled = cave;
        if (filled.append({EditKind::deposit, deposit, 0u}) != AppendStatus::applied
            || !near(filled.sample({}), -0.5) || filled.sample({}).material() != 5u
            || filled.sample({1.0f, 0.0f, 0.0f}).region() != Region::outside) return 6;
        EditVolume reverse{rock};
        if (reverse.append({EditKind::deposit, deposit, 0u}) != AppendStatus::applied
            || reverse.append({EditKind::subtract, cavity, 3u}) != AppendStatus::applied
            || reverse.sample({}).region() != Region::outside) return 6;

        EditVolume repeated{rock};
        if (repeated.append({EditKind::subtract, cavity, 3u}) != AppendStatus::applied
            || repeated.append({EditKind::deposit, deposit, 0u}) != AppendStatus::applied) return 7;
        for (const Float3 point : std::array{Float3{}, Float3{1.0f, 0.0f, 0.0f},
                Float3{0.0f, 2.0f, 0.0f}, Float3{0.0f, -3.0f, 0.0f}, Float3{7.0f, 3.0f, -2.0f}})
            if (!same_sample(filled.sample(point), repeated.sample(point))
                || !same_sample(filled.sample(point), filled.sample(point))) return 7;

        EditVolume bounded{Sphere{{}, 1.0f, 0u}};
        if (bounded.append({static_cast<EditKind>(255u), deposit, 0u}) != AppendStatus::invalid_edit
            || bounded.append({EditKind::deposit, Sphere{}, 0u}) != AppendStatus::invalid_edit
            || bounded.size() != 0u) return 8;
        for (std::size_t index = 0; index < EditVolume::maximum_edits; ++index)
            if (bounded.append({EditKind::deposit, deposit, 0u}) != AppendStatus::applied) return 8;
        const auto before = bounded.sample({});
        if (bounded.append({EditKind::subtract, cavity, 0u}) != AppendStatus::capacity_exceeded
            || bounded.size() != EditVolume::maximum_edits || !same_sample(before, bounded.sample({}))) return 8;
        EditVolume invalid{Sphere{}};
        if (invalid.valid() || invalid.sample({})
            || invalid.append({EditKind::deposit, deposit, 0u}) != AppendStatus::invalid_base
            || invalid.size() != 0u || bounded.sample({nan, 0.0f, 0.0f})) return 8;
        return 0;
    }
}

#if defined(EPOCH_VOXEL_SDF_CONTRACT_MAIN)
int main()
{
    return epochengine::voxel::sdf::contracts::run_contract();
}
#endif
