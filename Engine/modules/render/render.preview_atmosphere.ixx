// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Shared camera and lighting inputs for the native planetary sky passes.
module;

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

export module render.preview_atmosphere;

import render.preview_grid;
import render.lighting;

export namespace epochengine::previewgrid
{
    struct PlanetAtmosphereView final
    {
        Vec3 observer{}; // Body-relative position in planet radii.
        Vec3 forward{}, right{}, up{};
        Vec3 sun{};
        float thickness{}; // Atmosphere height in planet radii.
        bool sun_visible{};
    };

    [[nodiscard]] inline std::optional<PlanetAtmosphereView> planet_atmosphere_view(
        const PublishedSceneFrame& scene, float aspect) noexcept
    {
        const Camera& camera = scene.camera;
        if (!std::isfinite(aspect) || aspect <= 0.0f || !(camera.fovRadians > 0.0f && camera.fovRadians < 3.14159265f))
            return {};
        PlanetAtmosphereView view;
        view.forward = camera_forward(camera);
        const auto right = normalize(cross(view.forward, camera.up));
        const auto up = cross(right, view.forward);
        if (!(dot(right, right) > 0.5f && dot(up, up) > 0.5f)) return {};
        const float extent = std::tan(camera.fovRadians * 0.5f);
        view.right = scale(right, extent * aspect);
        view.up = scale(up, extent);

        const std::array<double, 3> eye{camera.eye.x, camera.eye.y, camera.eye.z};
        const double cone = std::atan(double(extent) * std::hypot(1.0, double(aspect)));
        double closest = std::numeric_limits<double>::infinity();
        for (const auto& marker : scene.markers)
        {
            if (!marker.planetAtmosphere) continue;
            const auto& candidate = *marker.planetAtmosphere;
            if (!std::isfinite(candidate.radius) || candidate.radius <= 0.0) continue;
            const double thickness = candidate.thickness / candidate.radius;
            if (!(thickness >= 1e-6 && thickness <= 0.2)) continue;
            std::array<double, 3> local{};
            for (std::size_t axis = 0; axis < local.size(); ++axis)
                local[axis] = ((scene.render_origin[axis] - candidate.world_center[axis]) + eye[axis]) / candidate.radius;
            const double distance = std::hypot(local[0], local[1], local[2]);
            if (!(distance < closest && distance < 96.0)) continue;
            const double outer = 1.0 + thickness;
            if (distance > outer)
            {
                const double facing = -(local[0] * view.forward.x + local[1] * view.forward.y
                    + local[2] * view.forward.z) / distance;
                // Use the diagonal cone so a limb in a viewport corner remains eligible.
                if (facing < std::cos(cone + std::asin(outer / distance))) continue;
            }
            closest = distance;
            view.observer = {float(local[0]), float(local[1]), float(local[2])};
            view.thickness = static_cast<float>(thickness);
        }
        if (view.thickness == 0.0f && !scene.space_background) return {};

        view.sun = {0.0f, 1.0f, 0.0f};
        float intensity{};
        if (scene.solids)
        {
            for (const auto& light : scene.solids->lighting.lights)
            {
                const auto& desc = light.desc;
                if (!desc.enabled || desc.kind != lighting::LightKind::Directional || desc.intensity <= intensity)
                    continue;
                intensity = desc.intensity;
                view.sun = normalize({-desc.direction.x, -desc.direction.y, -desc.direction.z});
            }
        }
        view.sun_visible = intensity > 0.0f;
        return view;
    }
}
