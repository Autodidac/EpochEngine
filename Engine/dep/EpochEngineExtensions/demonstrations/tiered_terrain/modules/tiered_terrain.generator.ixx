// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

export module epoch_extensions.tiered_terrain;

import terrain.foundation;

export namespace epochengine::extensions::tiered_terrain
{
    struct PackageCapabilities final
    {
        bool deterministicLocalHeightfield{true};
        bool automaticExecution{};
        bool nativeRenderer{};
        bool planetaryTerrain{};

        friend constexpr bool operator==(
            const PackageCapabilities&,
            const PackageCapabilities&) noexcept = default;
    };

    inline constexpr PackageCapabilities kCapabilities{};

    struct PackageLimits final
    {
        std::uint32_t minimumAxisSamples{5};
        std::uint32_t maximumAxisSamples{129};
        std::uint64_t maximumTotalSamples{16641};
        std::uint32_t minimumTerraces{2};
        std::uint32_t maximumTerraces{32};
        float maximumSampleSpacing{256.0f};
        float maximumTerraceHeight{512.0f};
        float maximumWorldSpan{8192.0f};
        float maximumHeightSpan{4096.0f};

        friend constexpr bool operator==(
            const PackageLimits&,
            const PackageLimits&) noexcept = default;
    };

    inline constexpr PackageLimits kPackageLimits{};

    struct Request final
    {
        terrain::TerrainAssetId asset{
            terrain::stable_asset_id("epoch.extensions.demo_tiered_terrain")};
        std::uint64_t seed{0x5449455245443031ull};
        terrain::Float3 origin{-32.0f, 0.0f, -32.0f};
        std::uint32_t widthSamples{65};
        std::uint32_t depthSamples{65};
        float sampleSpacing{1.0f};
        std::uint32_t terraceCount{9};
        float terraceHeight{1.5f};
        std::uint32_t materialSlot{};
        bool collisionQueries{true};

        friend constexpr bool operator==(
            const Request&,
            const Request&) noexcept = default;
    };

    enum class Status : std::uint8_t
    {
        ready,
        invalid_request,
        package_capacity_exceeded,
        core_rejected
    };

    struct Result final
    {
        Status status{Status::invalid_request};
        terrain::TerrainStatus coreStatus{
            terrain::TerrainStatus::invalid_descriptor};
        terrain::Heightfield heightfield{};
        std::uint32_t minimumTerrace{};
        std::uint32_t maximumTerrace{};
        std::uint32_t realizedTerraceCount{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return status == Status::ready;
        }
    };

    namespace detail
    {
        [[nodiscard]] constexpr std::uint64_t mix64(
            std::uint64_t value) noexcept
        {
            value ^= value >> 30u;
            value *= 0xbf58476d1ce4e5b9ull;
            value ^= value >> 27u;
            value *= 0x94d049bb133111ebull;
            value ^= value >> 31u;
            return value;
        }

        [[nodiscard]] constexpr std::uint32_t coordinate_noise(
            std::uint64_t seed,
            std::uint32_t x,
            std::uint32_t z) noexcept
        {
            const std::uint64_t coordinate =
                static_cast<std::uint64_t>(x) * 0x9e3779b185ebca87ull ^
                static_cast<std::uint64_t>(z) * 0xc2b2ae3d27d4eb4full;
            return static_cast<std::uint32_t>(mix64(seed ^ coordinate) >> 32u);
        }

        [[nodiscard]] constexpr std::uint32_t normalized_distance(
            std::uint32_t coordinate,
            std::uint32_t extent) noexcept
        {
            const std::uint64_t doubled =
                static_cast<std::uint64_t>(coordinate) * 2ull;
            const std::uint64_t maximum = extent - 1u;
            const std::uint64_t distance =
                doubled > maximum ? doubled - maximum : maximum - doubled;
            return static_cast<std::uint32_t>(
                (distance * 65535ull) / maximum);
        }

        [[nodiscard]] constexpr std::uint32_t terrace_at(
            const Request& request,
            std::uint32_t x,
            std::uint32_t z) noexcept
        {
            const bool border =
                x == 0u || z == 0u ||
                x + 1u == request.widthSamples ||
                z + 1u == request.depthSamples;
            if (border)
            {
                return 0u;
            }

            const std::uint32_t centerX = request.widthSamples / 2u;
            const std::uint32_t centerZ = request.depthSamples / 2u;
            if (x == centerX && z == centerZ)
            {
                return request.terraceCount - 1u;
            }

            const std::uint32_t distance = (std::max)(
                normalized_distance(x, request.widthSamples),
                normalized_distance(z, request.depthSamples));
            const std::uint32_t dome = 65535u - distance;
            const std::uint32_t broadNoise =
                coordinate_noise(request.seed, x / 8u, z / 8u) & 0xffffu;
            const std::uint32_t detailNoise = coordinate_noise(
                request.seed ^ 0x7465727261636531ull,
                x / 3u,
                z / 3u) & 0xffffu;
            const std::uint64_t score =
                static_cast<std::uint64_t>(dome) * 3ull +
                broadNoise + detailNoise;
            const std::uint64_t range = 5ull * 65536ull;
            const std::uint32_t terrace = static_cast<std::uint32_t>(
                (score * request.terraceCount) / range);
            return (std::min)(terrace, request.terraceCount - 1u);
        }

        [[nodiscard]] inline Status validate_request(
            const Request& request) noexcept
        {
            const bool finite =
                std::isfinite(request.origin.x) &&
                std::isfinite(request.origin.y) &&
                std::isfinite(request.origin.z) &&
                std::isfinite(request.sampleSpacing) &&
                std::isfinite(request.terraceHeight);
            if (!request.asset.valid() ||
                !finite ||
                request.sampleSpacing <= 0.0f ||
                request.terraceHeight <= 0.0f ||
                request.widthSamples < kPackageLimits.minimumAxisSamples ||
                request.depthSamples < kPackageLimits.minimumAxisSamples ||
                request.terraceCount < kPackageLimits.minimumTerraces)
            {
                return Status::invalid_request;
            }

            const std::uint64_t totalSamples =
                static_cast<std::uint64_t>(request.widthSamples) *
                request.depthSamples;
            const float widthWorld =
                static_cast<float>(request.widthSamples - 1u) *
                request.sampleSpacing;
            const float depthWorld =
                static_cast<float>(request.depthSamples - 1u) *
                request.sampleSpacing;
            const float heightWorld =
                static_cast<float>(request.terraceCount - 1u) *
                request.terraceHeight;
            if (request.widthSamples > kPackageLimits.maximumAxisSamples ||
                request.depthSamples > kPackageLimits.maximumAxisSamples ||
                totalSamples > kPackageLimits.maximumTotalSamples ||
                request.terraceCount > kPackageLimits.maximumTerraces ||
                request.sampleSpacing > kPackageLimits.maximumSampleSpacing ||
                request.terraceHeight > kPackageLimits.maximumTerraceHeight ||
                !std::isfinite(widthWorld) ||
                !std::isfinite(depthWorld) ||
                !std::isfinite(heightWorld) ||
                widthWorld > kPackageLimits.maximumWorldSpan ||
                depthWorld > kPackageLimits.maximumWorldSpan ||
                heightWorld > kPackageLimits.maximumHeightSpan)
            {
                return Status::package_capacity_exceeded;
            }
            return Status::ready;
        }
    }

    [[nodiscard]] inline Result generate(
        const Request& request,
        const terrain::TerrainLimits& coreLimits = {})
    {
        const Status requestStatus = detail::validate_request(request);
        if (requestStatus != Status::ready)
        {
            return {.status = requestStatus};
        }

        const std::size_t sampleCount =
            static_cast<std::size_t>(request.widthSamples) *
            request.depthSamples;
        std::vector<float> normalizedSamples(sampleCount, 0.0f);
        std::uint32_t minimumTerrace = request.terraceCount - 1u;
        std::uint32_t maximumTerrace{};
        std::uint32_t realizedMask{};
        const float denominator =
            static_cast<float>(request.terraceCount - 1u);

        for (std::uint32_t z = 0; z < request.depthSamples; ++z)
        {
            for (std::uint32_t x = 0; x < request.widthSamples; ++x)
            {
                const std::uint32_t terrace =
                    detail::terrace_at(request, x, z);
                minimumTerrace = (std::min)(minimumTerrace, terrace);
                maximumTerrace = (std::max)(maximumTerrace, terrace);
                realizedMask |= std::uint32_t{1u} << terrace;
                normalizedSamples[
                    static_cast<std::size_t>(z) * request.widthSamples + x] =
                    static_cast<float>(terrace) / denominator;
            }
        }

        terrain::HeightfieldDescriptor descriptor{};
        descriptor.asset = request.asset;
        descriptor.kind = terrain::FoundationKind::local_heightfield;
        descriptor.origin = request.origin;
        descriptor.widthSamples = request.widthSamples;
        descriptor.depthSamples = request.depthSamples;
        descriptor.sampleSpacing = request.sampleSpacing;
        descriptor.baseHeight = 0.0f;
        descriptor.heightScale =
            static_cast<float>(request.terraceCount - 1u) *
            request.terraceHeight;
        descriptor.materialSlot = request.materialSlot;
        descriptor.collisionQueries = request.collisionQueries;

        terrain::HeightfieldBuildResult coreResult =
            terrain::make_heightfield(
                descriptor,
                normalizedSamples,
                coreLimits);
        if (!coreResult)
        {
            return {
                .status = Status::core_rejected,
                .coreStatus = coreResult.status
            };
        }

        return {
            .status = Status::ready,
            .coreStatus = terrain::TerrainStatus::ready,
            .heightfield = std::move(coreResult.heightfield),
            .minimumTerrace = minimumTerrace,
            .maximumTerrace = maximumTerrace,
            .realizedTerraceCount =
                static_cast<std::uint32_t>(std::popcount(realizedMask))
        };
    }
}
