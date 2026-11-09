// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

module water.surface;

namespace epochengine::water::surface
{
    WaterBodyDesc default_body() noexcept
    {
        return {.halfExtents={8.0f,4.0f,8.0f},.waveAmplitude=0.35f,
            .wavelength=4.0f,.waveSpeed=1.4f,.waveDirection={1.0f,0.0f,0.35f},
            .secondaryWave={.amplitude=0.20f,.wavelength=2.7f,.speed=1.1f,
                .direction={-0.35f,0.0f,1.0f},.phase=0.8f}};
    }

    namespace
    {
        [[nodiscard]] float coordinate(float center, float extent, double fraction) noexcept
        {
            float value = static_cast<float>(double(center) + double(extent) * (2.0 * fraction - 1.0));
            // A rounded endpoint can sit just outside contains_xz's exact
            // float boundary. Move it toward the body's interior once.
            if (std::isfinite(value) && std::abs(value - center) > extent)
                value = std::nextafter(value, center);
            return value;
        }
    }

    [[nodiscard]] asset::mesh::Result build(
        const WaterBodyDesc& body,
        double sample_time,
        std::uint32_t cells) noexcept
    {
        using asset::mesh::Code;
        if (cells < 2u || cells > 64u)
            return {Code::limit_exceeded};
        if (!validate(body).valid || !body.enabled || !std::isfinite(sample_time) ||
            (body.kind != WaterBodyKind::PlaneRegion && body.kind != WaterBodyKind::BoxVolume))
            return {Code::invalid_geometry};

        try
        {
            WaterManager manager{1};
            const auto handle = manager.create(body);
            if (!handle.valid())
                return {Code::invalid_geometry};
            const auto admitted = manager.get(handle);
            if (!admitted)
                return {Code::invalid_geometry};

            const std::uint32_t stride = cells + 1u;
            std::vector<asset::mesh::Vertex> vertices;
            std::vector<std::uint32_t> indices;
            vertices.reserve(std::size_t(stride) * stride);
            indices.reserve(std::size_t(cells) * cells * 6u);
            for (std::uint32_t z = 0; z <= cells; ++z)
            {
                const double v = double(z) / cells;
                for (std::uint32_t x = 0; x <= cells; ++x)
                {
                    const double u = double(x) / cells;
                    const Float3 position{
                        coordinate(admitted->center.x, admitted->halfExtents.x, u),
                        admitted->center.y,
                        coordinate(admitted->center.z, admitted->halfExtents.z, v)};
                    const auto sample = manager.sample(handle, {position, sample_time});
                    if (!sample.found)
                        return {Code::invalid_geometry};
                    const float totalAmplitude = admitted->waveAmplitude + admitted->secondaryWave.amplitude;
                    const float crest = totalAmplitude > 0.0f
                        ? std::clamp(0.5f + 0.5f * ((sample.surfaceHeight - admitted->surfaceHeight) /
                                                  totalAmplitude), 0.0f, 1.0f)
                        : 0.5f;
                    vertices.push_back({
                        .position = {position.x+sample.displacement.x, sample.surfaceHeight, position.z+sample.displacement.z},
                        .normal = {sample.surfaceNormal.x, sample.surfaceNormal.y, sample.surfaceNormal.z},
                        .uv = {float(u), float(v)},
                        .color = {0.04f + 0.10f * crest, 0.22f + 0.26f * crest, 0.48f + 0.35f * crest}});
                }
            }
            for (std::uint32_t z = 0; z < cells; ++z)
            {
                for (std::uint32_t x = 0; x < cells; ++x)
                {
                    const auto a = z * stride + x;
                    const auto b = a + 1u;
                    const auto c = a + stride;
                    const auto d = c + 1u;
                    indices.insert(indices.end(), {a, c, b, b, c, d});
                }
            }
            // The fixed 64-cell ceiling needs fewer than 300KiB encoded;
            // the canonical compiler independently enforces its default4MiB.
            return asset::mesh::build_artifact(vertices, indices);
        }
        catch (...)
        {
            return {Code::allocation_failed};
        }
    }
}
