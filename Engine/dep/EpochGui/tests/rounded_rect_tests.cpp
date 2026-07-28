#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>

import epoch.gui.rounded_rect;

namespace
{
    namespace gui = epochengine::gui_lib;
    namespace rounded = epochengine::gui_lib::rounded_rect;

    [[nodiscard]] bool indices_valid(
        std::span<const std::uint32_t> indices,
        std::size_t vertex_count) noexcept
    {
        return indices.size() % 3U == 0U
            && std::ranges::all_of(indices, [vertex_count](std::uint32_t index) noexcept
            {
                return index < vertex_count;
            });
    }

    [[nodiscard]] bool mesh_valid(const rounded::RoundedRectMesh& mesh) noexcept
    {
        if (!mesh.valid || mesh.outer_contour().empty())
            return false;
        if (!indices_valid(mesh.fill_indices, mesh.vertices.size()))
            return false;
        if (!indices_valid(mesh.border_indices, mesh.vertices.size()))
            return false;

        constexpr float epsilon = 0.01f;
        const float width = mesh.bounds.size.x;
        const float height = mesh.bounds.size.y;

        return mesh.radii.top_left + mesh.radii.top_right <= width + epsilon
            && mesh.radii.bottom_left + mesh.radii.bottom_right <= width + epsilon
            && mesh.radii.top_left + mesh.radii.bottom_left <= height + epsilon
            && mesh.radii.top_right + mesh.radii.bottom_right <= height + epsilon
            && std::ranges::all_of(mesh.vertices, [](gui::Vec2 point) noexcept
            {
                return std::isfinite(point.x) && std::isfinite(point.y);
            });
    }
}

int main()
{
    constexpr std::array cases{
        rounded::RoundedRectOptions{
            .bounds = { { 0.0f, 0.0f }, { 360.0f, 120.0f } },
            .radii = { 24.0f, 24.0f, 24.0f, 24.0f },
            .border_width = 4.0f,
            .segments_per_corner = 12
        },
        rounded::RoundedRectOptions{
            .bounds = { { 0.0f, 0.0f }, { 360.0f, 88.0f } },
            .radii = { 44.0f, 44.0f, 44.0f, 44.0f },
            .border_width = 3.0f,
            .segments_per_corner = 16
        },
        rounded::RoundedRectOptions{
            .bounds = { { 0.0f, 0.0f }, { 360.0f, 150.0f } },
            .radii = { 8.0f, 44.0f, 12.0f, 36.0f },
            .border_width = 5.0f,
            .segments_per_corner = 12
        },
        rounded::RoundedRectOptions{
            .bounds = { { 0.0f, 0.0f }, { 360.0f, 130.0f } },
            .radii = { 160.0f, 120.0f, 180.0f, 140.0f },
            .border_width = 6.0f,
            .segments_per_corner = 16
        },
        rounded::RoundedRectOptions{
            .bounds = { { 0.0f, 0.0f }, { 0.0f, 0.0f } },
            .radii = { -1.0f, -2.0f, -3.0f, -4.0f },
            .border_width = -8.0f,
            .segments_per_corner = 0
        }
    };

    for (std::size_t index = 0; index + 1U < cases.size(); ++index)
    {
        if (!mesh_valid(rounded::make_rounded_rect_mesh(cases[index])))
            return static_cast<int>(index + 1U);
    }

    const auto empty = rounded::make_rounded_rect_mesh(cases.back());
    return empty.valid ? 10 : 0;
}
