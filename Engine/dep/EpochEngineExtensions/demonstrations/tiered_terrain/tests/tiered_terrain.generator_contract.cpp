// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <cmath>
#include <cstdint>
#include <vector>

import epoch_extensions.tiered_terrain;
import terrain.foundation;

namespace
{
    [[nodiscard]] bool nearly_equal(
        float lhs,
        float rhs,
        float tolerance = 0.0001f) noexcept
    {
        return std::abs(lhs - rhs) <= tolerance;
    }
}

int main()
{
    namespace package = epochengine::extensions::tiered_terrain;
    namespace terrain = epochengine::terrain;

    if (!package::kCapabilities.deterministicLocalHeightfield ||
        package::kCapabilities.automaticExecution ||
        package::kCapabilities.nativeRenderer ||
        package::kCapabilities.planetaryTerrain ||
        terrain::kFoundationCapabilities.nativeRendering ||
        terrain::kFoundationCapabilities.planetaryTerrain)
    {
        return 1;
    }

    package::Request request{};
    request.origin = {-40.0f, -3.0f, 12.0f};
    request.widthSamples = 65;
    request.depthSamples = 49;
    request.sampleSpacing = 1.25f;
    request.terraceCount = 11;
    request.terraceHeight = 2.0f;
    request.materialSlot = 7;

    const package::Result first = package::generate(request);
    const package::Result repeated = package::generate(request);
    if (!first || !repeated ||
        first.coreStatus != terrain::TerrainStatus::ready ||
        !first.heightfield.valid() ||
        first.heightfield != repeated.heightfield)
    {
        return 2;
    }

    package::Request changedRequest = request;
    changedRequest.seed ^= 0xd1b54a32d192ed03ull;
    const package::Result changed = package::generate(changedRequest);
    if (!changed ||
        changed.heightfield.contentHash == first.heightfield.contentHash ||
        changed.heightfield.normalizedSamples ==
            first.heightfield.normalizedSamples)
    {
        return 3;
    }

    if (first.minimumTerrace != 0u ||
        first.maximumTerrace != request.terraceCount - 1u ||
        first.realizedTerraceCount < 4u ||
        first.realizedTerraceCount > request.terraceCount)
    {
        return 4;
    }
    for (const float sample : first.heightfield.normalizedSamples)
    {
        const float terrace =
            sample * static_cast<float>(request.terraceCount - 1u);
        if (!nearly_equal(terrace, std::round(terrace)))
        {
            return 5;
        }
    }

    const terrain::TerrainBounds& bounds = first.heightfield.bounds;
    const float widthWorld =
        static_cast<float>(request.widthSamples - 1u) *
        request.sampleSpacing;
    const float depthWorld =
        static_cast<float>(request.depthSamples - 1u) *
        request.sampleSpacing;
    const float heightWorld =
        static_cast<float>(request.terraceCount - 1u) *
        request.terraceHeight;
    if (!nearly_equal(bounds.minimum.x, request.origin.x) ||
        !nearly_equal(bounds.minimum.y, request.origin.y) ||
        !nearly_equal(bounds.minimum.z, request.origin.z) ||
        !nearly_equal(bounds.maximum.x, request.origin.x + widthWorld) ||
        !nearly_equal(bounds.maximum.y, request.origin.y + heightWorld) ||
        !nearly_equal(bounds.maximum.z, request.origin.z + depthWorld))
    {
        return 6;
    }

    const auto meshPlan = terrain::make_mesh_plan(first.heightfield);
    const std::uint64_t cellCount =
        static_cast<std::uint64_t>(request.widthSamples - 1u) *
        (request.depthSamples - 1u);
    if (!meshPlan ||
        meshPlan->vertexCount !=
            static_cast<std::uint64_t>(request.widthSamples) *
                request.depthSamples ||
        meshPlan->triangleCount != cellCount * 2ull ||
        meshPlan->indexCount != cellCount * 6ull)
    {
        return 7;
    }

    const float centerX = request.origin.x +
        static_cast<float>(request.widthSamples / 2u) *
        request.sampleSpacing;
    const float centerZ = request.origin.z +
        static_cast<float>(request.depthSamples / 2u) *
        request.sampleSpacing;
    const auto center = terrain::sample_surface(
        first.heightfield,
        centerX,
        centerZ);
    if (!center ||
        !nearly_equal(center->position.y, request.origin.y + heightWorld) ||
        center->materialSlot != request.materialSlot)
    {
        return 8;
    }

    package::Request oversized = request;
    oversized.widthSamples = package::kPackageLimits.maximumAxisSamples + 1u;
    if (package::generate(oversized).status !=
        package::Status::package_capacity_exceeded)
    {
        return 9;
    }

    package::Request invalid = request;
    invalid.terraceHeight = 0.0f;
    if (package::generate(invalid).status !=
        package::Status::invalid_request)
    {
        return 10;
    }

    terrain::TerrainLimits constrainedCore{};
    constrainedCore.maximumAxisSamples = 16;
    const package::Result rejected =
        package::generate(request, constrainedCore);
    if (rejected.status != package::Status::core_rejected ||
        rejected.coreStatus != terrain::TerrainStatus::capacity_exceeded ||
        rejected.heightfield.valid())
    {
        return 11;
    }

    return 0;
}
