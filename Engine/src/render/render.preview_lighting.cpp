// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

module render.preview_grid;
import asset.mesh_artifact;
import asset.texture_artifact;
import render.lighting;
import render.texture_artifact;

namespace epochengine::previewgrid
{
    namespace detail
    {
        // A small light-space depth map for the existing shared reference
        // lighting path. Use the tree's real triangles, including foliage
        // cutouts, and exclude translation so camera-origin changes reuse it.
        struct ForestShadow final
        {
            static constexpr int size = 48;
            static constexpr std::size_t maxIndices = 34000u;
            std::array<float, size * size> depth{};
            Vec3 right{}, up{}, towardLight{}, minimum{};
            float xScale{}, yScale{}, bias{};

            [[nodiscard]] Vec3 project(Vec3 local) const noexcept
            {
                return {dot(local, right), dot(local, up), dot(local, towardLight)};
            }

            [[nodiscard]] Vec3 pixels(Vec3 point) const noexcept
            {
                return {(point.x - minimum.x) * xScale + 1.0f,
                    (point.y - minimum.y) * yScale + 1.0f, point.z};
            }

            [[nodiscard]] float visibility(Vec3 local) const noexcept
            {
                if (xScale == 0.0f || yScale == 0.0f) return 1.0f;
                const auto p = pixels(project(local));
                const auto x = (std::clamp)(static_cast<int>(std::floor(p.x - 0.5f)), 0, size - 2);
                const auto y = (std::clamp)(static_cast<int>(std::floor(p.y - 0.5f)), 0, size - 2);
                const auto fx = (std::clamp)(p.x - static_cast<float>(x) - 0.5f, 0.0f, 1.0f);
                const auto fy = (std::clamp)(p.y - static_cast<float>(y) - 0.5f, 0.0f, 1.0f);
                const auto visible = [&](int column, int row) {
                    return p.z + bias >= depth[row * size + column] ? 1.0f : 0.0f;
                };
                return std::lerp(std::lerp(visible(x, y), visible(x + 1, y), fx),
                    std::lerp(visible(x, y + 1), visible(x + 1, y + 1), fx), fy);
            }

            void build(const ObjectMarker& localMarker, const EulerRotation& rotation, Vec3 lightDirection)
            {
                xScale = yScale = 0.0f;
                towardLight = normalize(scale(lightDirection, -1.0f));
                const auto reference = std::abs(towardLight.y) < 0.95f ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
                right = normalize(cross(reference, towardLight));
                up = cross(towardLight, right);
                const auto vertices = localMarker.mesh->vertices();
                const auto indices = localMarker.mesh->indices();
                std::vector<Vec3> projected;
                projected.reserve(vertices.size());
                for (const auto& vertex : vertices)
                    projected.push_back(project(mesh_vertex_position(localMarker, vertex.position, rotation)));
                minimum = projected.front();
                auto maximum = minimum;
                for (const auto& point : projected)
                {
                    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) return;
                    minimum.x = (std::min)(minimum.x, point.x);
                    minimum.y = (std::min)(minimum.y, point.y);
                    maximum.x = (std::max)(maximum.x, point.x);
                    maximum.y = (std::max)(maximum.y, point.y);
                }
                const auto width = maximum.x - minimum.x, height = maximum.y - minimum.y;
                if (!std::isfinite(width) || !std::isfinite(height) || width <= 1e-6f || height <= 1e-6f) return;
                xScale = (size - 2.0f) / width;
                yScale = (size - 2.0f) / height;
                bias = 0.65f * (std::max)(width, height) / (size - 2.0f);
                for (auto& point : projected) point = pixels(point);
                depth.fill(-(std::numeric_limits<float>::infinity)());
                const auto& binding = localMarker.baseColorTexture;
                const auto* texture = binding && binding->alpha_cutout ? binding->artifact.artifact() : nullptr;
                const auto* mip = texture && !texture->mips.empty() ? &texture->mips.front() : nullptr;
                const auto covers = [&](const auto& a, const auto& b, const auto& c, float u, float v, float w) {
                    // Bark is opaque. Forest foliage has padded UVs wholly
                    // inside the right half of its atlas.
                    if (!mip || a.uv[0] < 0.5f || b.uv[0] < 0.5f || c.uv[0] < 0.5f) return true;
                    const auto coord = [](float uv, std::uint32_t extent) {
                        return (std::min)(static_cast<std::uint32_t>((std::clamp)(uv, 0.0f, 1.0f)
                            * static_cast<float>(extent)), extent - 1u);
                    };
                    const auto x = coord(u * a.uv[0] + v * b.uv[0] + w * c.uv[0], mip->width);
                    const auto y = coord(u * a.uv[1] + v * b.uv[1] + w * c.uv[1], mip->height);
                    const auto alpha = std::to_integer<unsigned>(mip->texels[std::size_t{y} * mip->row_pitch_bytes + x * 4u + 3u]);
                    return alpha >= binding->alpha_cutoff * 255.0f;
                };
                for (std::size_t triangle = 0; triangle < indices.size(); triangle += 3u)
                {
                    const auto i = indices[triangle], j = indices[triangle + 1u], k = indices[triangle + 2u];
                    const auto a = projected[i], b = projected[j], c = projected[k];
                    const auto ab = subtract(b, a), ac = subtract(c, a);
                    const auto area = ab.x * ac.y - ab.y * ac.x;
                    if (std::abs(area) < 1e-6f) continue;
                    const auto minX = (std::max)(0, static_cast<int>(std::ceil((std::min)({a.x, b.x, c.x}) - 0.5f)));
                    const auto maxX = (std::min)(size - 1, static_cast<int>(std::floor((std::max)({a.x, b.x, c.x}) - 0.5f)));
                    const auto minY = (std::max)(0, static_cast<int>(std::ceil((std::min)({a.y, b.y, c.y}) - 0.5f)));
                    const auto maxY = (std::min)(size - 1, static_cast<int>(std::floor((std::max)({a.y, b.y, c.y}) - 0.5f)));
                    const auto inverseArea = 1.0f / area;
                    for (int y = minY; y <= maxY; ++y)
                        for (int x = minX; x <= maxX; ++x)
                        {
                            const auto dx = static_cast<float>(x) + 0.5f - a.x;
                            const auto dy = static_cast<float>(y) + 0.5f - a.y;
                            const auto v = (dx * ac.y - dy * ac.x) * inverseArea;
                            const auto w = (ab.x * dy - ab.y * dx) * inverseArea;
                            const auto u = 1.0f - v - w;
                            if (u < 0.0f || v < 0.0f || w < 0.0f) continue;
                            auto& stored = depth[y * size + x];
                            const auto candidate = u * a.z + v * b.z + w * c.z;
                            if (candidate > stored && covers(vertices[i], vertices[j], vertices[k], u, v, w))
                                stored = candidate;
                        }
                }
            }
        };

        [[nodiscard]] static const ForestShadow* forest_shadow_for(const ObjectMarker& localMarker,
            const EulerRotation& rotation, const epochengine::lighting::LightDesc& light)
        {
            if (localMarker.mesh->vertices().empty() || localMarker.mesh->vertices().size() > kMaxForestShadowVertices
                || localMarker.mesh->indices().size() > ForestShadow::maxIndices) return nullptr;
            struct Entry
            {
                std::array<std::uint64_t, 4u> mesh{}, texture{};
                std::array<float, 10u> pose{};
                std::optional<PreciseMeshTransform> precise{};
                ForestShadow shadow{};
                bool valid{};
            };
            // One bounded cache per preparation thread. No backend state or
            // mutable world resources are retained here.
            static thread_local std::array<Entry, 32u> entries;
            static thread_local std::size_t next{};
            const auto& binding = localMarker.baseColorTexture;
            const auto* artifact = binding && binding->alpha_cutout ? binding->artifact.artifact() : nullptr;
            const auto mesh = localMarker.mesh->key().words;
            const auto texture = artifact ? artifact->identity.key.words : decltype(mesh){};
            const std::array pose{localMarker.scale.x, localMarker.scale.y, localMarker.scale.z,
                localMarker.rotationDegrees.x, localMarker.rotationDegrees.y, localMarker.rotationDegrees.z,
                light.direction.x, light.direction.y, light.direction.z, artifact ? binding->alpha_cutoff : 0.0f};
            for (const auto& entry : entries)
                if (entry.valid && entry.mesh == mesh && entry.texture == texture && entry.pose == pose
                    && entry.precise == localMarker.preciseTransform)
                    return &entry.shadow;
            auto& entry = entries[next++ % entries.size()];
            entry.valid = false;
            entry.shadow.build(localMarker, rotation, {light.direction.x, light.direction.y, light.direction.z});
            entry.mesh = mesh;
            entry.texture = texture;
            entry.pose = pose;
            entry.precise = localMarker.preciseTransform;
            entry.valid = true;
            return &entry.shadow;
        }
        [[nodiscard]] const ForestShadow* prepare_forest_shadow(ObjectMarker& localMarker,
            const EulerRotation& rotation, const epochengine::lighting::LightingFrame& frame,
            const epochengine::lighting::LightDesc*& sun)
        {
            for (const auto& light : frame.lights)
                if (light.desc.enabled && light.desc.kind == epochengine::lighting::LightKind::Directional
                    && light.desc.intensity > (sun ? sun->intensity : 0.0f))
                    sun = &light.desc;
            if (!sun) return nullptr;
            localMarker.position = {};
            if (localMarker.preciseTransform) localMarker.preciseTransform->position = {};
            return forest_shadow_for(localMarker, rotation, *sun);
        }

        [[nodiscard]] float forest_shadow_visibility(const ForestShadow& shadow, Vec3 local) noexcept
        {
            return shadow.visibility(local);
        }

        [[nodiscard]] bool same_solid_lighting(const epochengine::lighting::LightingFrame& a,
            const epochengine::lighting::LightingFrame& b) noexcept
        {
            if(a.revision!=b.revision||!epochengine::lighting::same_environment(a.environment,b.environment)
                ||a.lights.size()!=b.lights.size()) return false;
            for(std::size_t i=0;i<a.lights.size();++i)
                if(!epochengine::lighting::same_light_desc(a.lights[i].desc,b.lights[i].desc)) return false;
            return true;
        }
        [[nodiscard]] bool same_eye(Vec3 a,Vec3 b) noexcept
        {return a.x==b.x&&a.y==b.y&&a.z==b.z;}
        [[nodiscard]] bool solid_uses_eye(const ObjectMarker& marker,
            const epochengine::lighting::LightingFrame& lights) noexcept
        {
            return marker.primitive==ObjectPreviewPrimitive::EngineArcadeCabinet
                ||(!marker.forestLighting&&std::ranges::any_of(lights.lights,[](const auto& light){
                    return light.desc.enabled&&light.desc.intensity>0.0f;}));
        }
    }
}
