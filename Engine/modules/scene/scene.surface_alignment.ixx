// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <cmath>
#include <optional>

export module scene.surface_alignment;

export namespace epochengine::scene::surface_alignment
{
    struct VerticalBounds final
    {
        float minimum_y{};
        float maximum_y{};

        [[nodiscard]] bool valid() const noexcept
        {
            return std::isfinite(minimum_y)
                && std::isfinite(maximum_y)
                && minimum_y <= maximum_y;
        }

        friend constexpr bool operator==(
            const VerticalBounds&,
            const VerticalBounds&) noexcept = default;
    };

    inline constexpr VerticalBounds centered_unit_bounds{-0.5f, 0.5f};
    inline constexpr VerticalBounds base_anchored_unit_bounds{0.0f, 1.0f};

    struct AlignmentResult final
    {
        float origin_y{};
        float translation_y{};
        VerticalBounds world_bounds{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return std::isfinite(origin_y)
                && std::isfinite(translation_y)
                && world_bounds.valid();
        }
    };

    [[nodiscard]] inline std::optional<VerticalBounds>
        transform_vertical_bounds(
            VerticalBounds local_bounds,
            float origin_y,
            float scale_y) noexcept
    {
        if (!local_bounds.valid()
            || !std::isfinite(origin_y)
            || !std::isfinite(scale_y)
            || scale_y == 0.0f)
        {
            return std::nullopt;
        }

        const float first = origin_y + local_bounds.minimum_y * scale_y;
        const float second = origin_y + local_bounds.maximum_y * scale_y;
        if (!std::isfinite(first) || !std::isfinite(second))
            return std::nullopt;
        return VerticalBounds{
            (std::min)(first, second),
            (std::max)(first, second)};
    }

    [[nodiscard]] inline std::optional<VerticalBounds> merge_vertical_bounds(
        VerticalBounds first,
        VerticalBounds second) noexcept
    {
        if (!first.valid() || !second.valid())
            return std::nullopt;
        return VerticalBounds{
            (std::min)(first.minimum_y, second.minimum_y),
            (std::max)(first.maximum_y, second.maximum_y)};
    }

    [[nodiscard]] inline std::optional<float> support_surface_elevation(
        VerticalBounds local_bounds,
        float origin_y,
        float scale_y) noexcept
    {
        const auto world = transform_vertical_bounds(
            local_bounds,
            origin_y,
            scale_y);
        return world
            ? std::optional<float>{world->maximum_y}
            : std::nullopt;
    }

    [[nodiscard]] inline std::optional<AlignmentResult>
        align_bottom_to_surface(
            VerticalBounds local_bounds,
            float origin_y,
            float scale_y,
            float surface_elevation) noexcept
    {
        if (!std::isfinite(surface_elevation))
            return std::nullopt;

        const auto world = transform_vertical_bounds(
            local_bounds,
            origin_y,
            scale_y);
        if (!world)
            return std::nullopt;

        const float translation = surface_elevation - world->minimum_y;
        const float aligned_origin = origin_y + translation;
        const VerticalBounds aligned_bounds{
            world->minimum_y + translation,
            world->maximum_y + translation};
        const AlignmentResult result{
            .origin_y = aligned_origin,
            .translation_y = translation,
            .world_bounds = aligned_bounds};
        return result ? std::optional<AlignmentResult>{result} : std::nullopt;
    }

    struct ContractReport final
    {
        bool box_bounds{};
        bool plant_bounds{};
        bool terrain_bounds{};
        bool signed_ground_elevation{};
        bool idempotent{};

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return box_bounds
                && plant_bounds
                && terrain_bounds
                && signed_ground_elevation
                && idempotent;
        }
    };

    [[nodiscard]] inline ContractReport run_contract() noexcept
    {
        ContractReport report{};

        const auto box = align_bottom_to_surface(
            centered_unit_bounds,
            0.0f,
            2.0f,
            3.0f);
        report.box_bounds = box
            && box->origin_y == 4.0f
            && box->world_bounds == VerticalBounds{3.0f, 5.0f};

        const auto plant = align_bottom_to_surface(
            VerticalBounds{0.0f, 4.0f},
            0.0f,
            0.5f,
            -1.5f);
        report.plant_bounds = plant
            && plant->origin_y == -1.5f
            && plant->world_bounds == VerticalBounds{-1.5f, 0.5f};

        const auto terrain = support_surface_elevation(
            centered_unit_bounds,
            2.0f,
            -6.0f);
        report.terrain_bounds = terrain && *terrain == 5.0f;

        const auto below = align_bottom_to_surface(
            centered_unit_bounds,
            8.0f,
            -2.0f,
            -3.0f);
        const auto above = align_bottom_to_surface(
            centered_unit_bounds,
            -8.0f,
            2.0f,
            3.0f);
        report.signed_ground_elevation = below && above
            && below->world_bounds.minimum_y == -3.0f
            && above->world_bounds.minimum_y == 3.0f;

        const auto aligned_again = box
            ? align_bottom_to_surface(
                centered_unit_bounds,
                box->origin_y,
                2.0f,
                3.0f)
            : std::nullopt;
        report.idempotent = aligned_again
            && aligned_again->origin_y == box->origin_y
            && aligned_again->translation_y == 0.0f
            && aligned_again->world_bounds == box->world_bounds;
        return report;
    }
}
