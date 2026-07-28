// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

export module terrain.foundation;

import render.math;

export namespace epochengine::terrain
{
    using Float3 = epochengine::render_math::Float3;

    struct TerrainAssetId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return value != 0;
        }

        friend constexpr bool operator==(
            const TerrainAssetId&,
            const TerrainAssetId&) noexcept = default;
    };

    [[nodiscard]] constexpr std::uint64_t stable_hash(
        std::string_view text) noexcept
    {
        std::uint64_t hash = 14695981039346656037ull;
        for (const unsigned char character : text)
        {
            hash ^= character;
            hash *= 1099511628211ull;
        }
        return hash == 0 ? 1 : hash;
    }

    [[nodiscard]] constexpr TerrainAssetId stable_asset_id(
        std::string_view canonicalName) noexcept
    {
        return {stable_hash(canonicalName)};
    }

    inline constexpr TerrainAssetId kTier0GroundAssetId =
        stable_asset_id("epoch.terrain.tier0.ground");

    enum class FoundationKind : std::uint8_t
    {
        flat_platform,
        local_heightfield
    };

    enum class TerrainScaleClass : std::uint8_t
    {
        tier0_ground,
        local_heightfield,
        planetary_extension
    };

    [[nodiscard]] constexpr bool requires_extension(
        TerrainScaleClass scaleClass) noexcept
    {
        return scaleClass == TerrainScaleClass::planetary_extension;
    }

    struct TerrainFoundationCapabilities final
    {
        bool boundedHeightfields{true};
        bool deterministicMeshPlans{true};
        bool deterministicSurfaceQueries{true};
        bool nativeRendering{};
        bool planetaryTerrain{};
    };

    inline constexpr TerrainFoundationCapabilities kFoundationCapabilities{};

    struct HeightfieldDescriptor final
    {
        TerrainAssetId asset{kTier0GroundAssetId};
        FoundationKind kind{FoundationKind::flat_platform};
        Float3 origin{-8.0f, 0.0f, -8.0f};
        std::uint32_t widthSamples{2};
        std::uint32_t depthSamples{2};
        float sampleSpacing{16.0f};
        float baseHeight{};
        float heightScale{};
        std::uint32_t materialSlot{};
        bool collisionQueries{true};
    };

    struct TerrainLimits final
    {
        std::uint32_t maximumAxisSamples{513};
        std::uint64_t maximumTotalSamples{263169};
        std::uint64_t maximumApproximateBytes{4ull * 1024ull * 1024ull};
        float maximumWorldSpan{16384.0f};
        float maximumAbsoluteHeight{8192.0f};
    };

    enum class TerrainStatus : std::uint8_t
    {
        ready,
        invalid_limits,
        invalid_identity,
        invalid_descriptor,
        unsupported_kind,
        capacity_exceeded,
        sample_count_mismatch,
        non_finite_sample,
        sample_out_of_range,
        coordinate_out_of_range
    };

    struct TerrainValidation final
    {
        TerrainStatus status{TerrainStatus::invalid_descriptor};
        std::uint64_t sampleCount{};
        std::uint64_t approximateBytes{};
        float widthWorld{};
        float depthWorld{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return status == TerrainStatus::ready;
        }
    };

    [[nodiscard]] inline TerrainValidation validate(
        const HeightfieldDescriptor& descriptor,
        const TerrainLimits& limits = {}) noexcept
    {
        TerrainValidation result{};

        const bool limitsValid =
            limits.maximumAxisSamples >= 2 &&
            limits.maximumTotalSamples >= 4 &&
            limits.maximumApproximateBytes >= 4 * sizeof(float) &&
            std::isfinite(limits.maximumWorldSpan) &&
            limits.maximumWorldSpan > 0.0f &&
            std::isfinite(limits.maximumAbsoluteHeight) &&
            limits.maximumAbsoluteHeight > 0.0f;
        if (!limitsValid)
        {
            result.status = TerrainStatus::invalid_limits;
            return result;
        }

        if (!descriptor.asset.valid())
        {
            result.status = TerrainStatus::invalid_identity;
            return result;
        }

        if (descriptor.kind != FoundationKind::flat_platform &&
            descriptor.kind != FoundationKind::local_heightfield)
        {
            result.status = TerrainStatus::unsupported_kind;
            return result;
        }

        const bool finiteDescriptor =
            epochengine::render_math::finite(descriptor.origin) &&
            std::isfinite(descriptor.sampleSpacing) &&
            std::isfinite(descriptor.baseHeight) &&
            std::isfinite(descriptor.heightScale);
        if (!finiteDescriptor ||
            descriptor.widthSamples < 2 ||
            descriptor.depthSamples < 2 ||
            descriptor.sampleSpacing <= 0.0f ||
            descriptor.heightScale < 0.0f)
        {
            result.status = TerrainStatus::invalid_descriptor;
            return result;
        }

        const std::uint64_t width = descriptor.widthSamples;
        const std::uint64_t depth = descriptor.depthSamples;
        result.sampleCount = width * depth;
        result.approximateBytes = result.sampleCount * sizeof(float);
        result.widthWorld =
            static_cast<float>(width - 1) * descriptor.sampleSpacing;
        result.depthWorld =
            static_cast<float>(depth - 1) * descriptor.sampleSpacing;

        const float minimumPossibleHeight =
            descriptor.origin.y +
            descriptor.baseHeight -
            descriptor.heightScale;
        const float maximumPossibleHeight =
            descriptor.origin.y +
            descriptor.baseHeight +
            descriptor.heightScale;
        const bool boundedDescriptor =
            descriptor.widthSamples <= limits.maximumAxisSamples &&
            descriptor.depthSamples <= limits.maximumAxisSamples &&
            result.sampleCount <= limits.maximumTotalSamples &&
            result.approximateBytes <= limits.maximumApproximateBytes &&
            std::isfinite(result.widthWorld) &&
            std::isfinite(result.depthWorld) &&
            result.widthWorld <= limits.maximumWorldSpan &&
            result.depthWorld <= limits.maximumWorldSpan &&
            std::isfinite(minimumPossibleHeight) &&
            std::isfinite(maximumPossibleHeight) &&
            std::abs(minimumPossibleHeight) <= limits.maximumAbsoluteHeight &&
            std::abs(maximumPossibleHeight) <= limits.maximumAbsoluteHeight;
        if (!boundedDescriptor)
        {
            result.status = TerrainStatus::capacity_exceeded;
            return result;
        }

        result.status = TerrainStatus::ready;
        return result;
    }

    struct TerrainBounds final
    {
        Float3 minimum{};
        Float3 maximum{};

        [[nodiscard]] bool valid() const noexcept
        {
            return
                epochengine::render_math::finite(minimum) &&
                epochengine::render_math::finite(maximum) &&
                minimum.x <= maximum.x &&
                minimum.y <= maximum.y &&
                minimum.z <= maximum.z;
        }
    };

    struct Heightfield final
    {
        HeightfieldDescriptor descriptor{};
        TerrainLimits admittedLimits{};
        std::vector<float> normalizedSamples{};
        std::uint64_t revision{};
        std::uint64_t contentHash{};
        TerrainBounds bounds{};

        [[nodiscard]] bool valid() const noexcept;
    };

    struct HeightfieldBuildResult final
    {
        TerrainStatus status{TerrainStatus::invalid_descriptor};
        Heightfield heightfield{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return status == TerrainStatus::ready;
        }
    };

    namespace detail
    {
        inline constexpr std::uint64_t kHashOffset = 14695981039346656037ull;
        inline constexpr std::uint64_t kHashPrime = 1099511628211ull;

        inline void hash_byte(
            std::uint64_t& hash,
            std::uint8_t value) noexcept
        {
            hash ^= value;
            hash *= kHashPrime;
        }

        template<typename Integer>
        inline void hash_integer(
            std::uint64_t& hash,
            Integer value) noexcept
        {
            using Unsigned = std::make_unsigned_t<Integer>;
            Unsigned bits = static_cast<Unsigned>(value);
            for (std::size_t byte = 0; byte < sizeof(Unsigned); ++byte)
            {
                hash_byte(
                    hash,
                    static_cast<std::uint8_t>(
                        (bits >> (byte * 8)) & static_cast<Unsigned>(0xff)));
            }
        }

        inline void hash_float(
            std::uint64_t& hash,
            float value) noexcept
        {
            hash_integer(hash, std::bit_cast<std::uint32_t>(value));
        }

        [[nodiscard]] inline float world_height(
            const HeightfieldDescriptor& descriptor,
            float normalizedSample) noexcept
        {
            return descriptor.baseHeight +
                normalizedSample * descriptor.heightScale;
        }

        [[nodiscard]] inline std::uint64_t compute_content_hash(
            const HeightfieldDescriptor& descriptor,
            std::span<const float> samples) noexcept
        {
            std::uint64_t hash = kHashOffset;
            hash_integer(hash, descriptor.asset.value);
            hash_integer(hash, static_cast<std::uint8_t>(descriptor.kind));
            hash_float(hash, descriptor.origin.x);
            hash_float(hash, descriptor.origin.y);
            hash_float(hash, descriptor.origin.z);
            hash_integer(hash, descriptor.widthSamples);
            hash_integer(hash, descriptor.depthSamples);
            hash_float(hash, descriptor.sampleSpacing);
            hash_float(hash, descriptor.baseHeight);
            hash_float(hash, descriptor.heightScale);
            hash_integer(hash, descriptor.materialSlot);
            hash_integer(hash, static_cast<std::uint8_t>(
                descriptor.collisionQueries ? 1 : 0));
            for (const float sample : samples)
            {
                hash_float(hash, sample);
            }
            return hash == 0 ? 1 : hash;
        }

        [[nodiscard]] inline TerrainBounds compute_bounds(
            const HeightfieldDescriptor& descriptor,
            std::span<const float> samples) noexcept
        {
            float minimumHeight = descriptor.baseHeight;
            float maximumHeight = descriptor.baseHeight;
            for (const float sample : samples)
            {
                const float height = world_height(descriptor, sample);
                minimumHeight = (std::min)(minimumHeight, height);
                maximumHeight = (std::max)(maximumHeight, height);
            }

            return {
                .minimum = {
                    descriptor.origin.x,
                    descriptor.origin.y + minimumHeight,
                    descriptor.origin.z
                },
                .maximum = {
                    descriptor.origin.x +
                        static_cast<float>(descriptor.widthSamples - 1) *
                        descriptor.sampleSpacing,
                    descriptor.origin.y + maximumHeight,
                    descriptor.origin.z +
                        static_cast<float>(descriptor.depthSamples - 1) *
                        descriptor.sampleSpacing
                }
            };
        }

        [[nodiscard]] inline std::size_t sample_index(
            const HeightfieldDescriptor& descriptor,
            std::uint32_t x,
            std::uint32_t z) noexcept
        {
            return
                static_cast<std::size_t>(z) * descriptor.widthSamples + x;
        }
    }

    [[nodiscard]] inline bool Heightfield::valid() const noexcept
    {
        const TerrainValidation descriptorValidation =
            validate(descriptor, admittedLimits);
        if (!descriptorValidation ||
            descriptorValidation.sampleCount != normalizedSamples.size() ||
            revision == 0 ||
            contentHash == 0 ||
            !bounds.valid())
        {
            return false;
        }

        for (const float sample : normalizedSamples)
        {
            if (!std::isfinite(sample) || sample < -1.0f || sample > 1.0f)
            {
                return false;
            }
        }

        if (contentHash != detail::compute_content_hash(
                descriptor,
                normalizedSamples))
        {
            return false;
        }

        const TerrainBounds expected = detail::compute_bounds(
            descriptor,
            normalizedSamples);
        return
            bounds.minimum == expected.minimum &&
            bounds.maximum == expected.maximum;
    }

    [[nodiscard]] inline HeightfieldBuildResult make_heightfield(
        HeightfieldDescriptor descriptor,
        std::span<const float> normalizedSamples = {},
        const TerrainLimits& limits = {})
    {
        const TerrainValidation descriptorValidation =
            validate(descriptor, limits);
        if (!descriptorValidation)
        {
            return {.status = descriptorValidation.status};
        }

        if (!normalizedSamples.empty() &&
            normalizedSamples.size() != descriptorValidation.sampleCount)
        {
            return {.status = TerrainStatus::sample_count_mismatch};
        }

        std::vector<float> samples(
            static_cast<std::size_t>(descriptorValidation.sampleCount),
            0.0f);
        if (!normalizedSamples.empty())
        {
            std::copy(
                normalizedSamples.begin(),
                normalizedSamples.end(),
                samples.begin());
        }

        for (const float sample : samples)
        {
            if (!std::isfinite(sample))
            {
                return {.status = TerrainStatus::non_finite_sample};
            }
            if (sample < -1.0f || sample > 1.0f)
            {
                return {.status = TerrainStatus::sample_out_of_range};
            }
        }

        Heightfield result{};
        result.descriptor = descriptor;
        result.admittedLimits = limits;
        result.normalizedSamples = std::move(samples);
        result.revision = 1;
        result.contentHash = detail::compute_content_hash(
            result.descriptor,
            result.normalizedSamples);
        result.bounds = detail::compute_bounds(
            result.descriptor,
            result.normalizedSamples);
        return {
            .status = TerrainStatus::ready,
            .heightfield = std::move(result)
        };
    }

    [[nodiscard]] inline TerrainStatus set_sample(
        Heightfield& heightfield,
        std::uint32_t x,
        std::uint32_t z,
        float normalizedHeight) noexcept
    {
        if (!heightfield.valid())
        {
            return TerrainStatus::invalid_descriptor;
        }
        if (x >= heightfield.descriptor.widthSamples ||
            z >= heightfield.descriptor.depthSamples)
        {
            return TerrainStatus::coordinate_out_of_range;
        }
        if (!std::isfinite(normalizedHeight))
        {
            return TerrainStatus::non_finite_sample;
        }
        if (normalizedHeight < -1.0f || normalizedHeight > 1.0f)
        {
            return TerrainStatus::sample_out_of_range;
        }

        const std::size_t index =
            detail::sample_index(heightfield.descriptor, x, z);
        if (heightfield.normalizedSamples[index] == normalizedHeight)
        {
            return TerrainStatus::ready;
        }

        heightfield.normalizedSamples[index] = normalizedHeight;
        ++heightfield.revision;
        if (heightfield.revision == 0)
        {
            heightfield.revision = 1;
        }
        heightfield.contentHash = detail::compute_content_hash(
            heightfield.descriptor,
            heightfield.normalizedSamples);
        heightfield.bounds = detail::compute_bounds(
            heightfield.descriptor,
            heightfield.normalizedSamples);
        return TerrainStatus::ready;
    }

    struct TerrainSurfaceSample final
    {
        Float3 position{};
        Float3 normal{0.0f, 1.0f, 0.0f};
        std::uint32_t materialSlot{};
    };

    [[nodiscard]] inline std::optional<TerrainSurfaceSample> sample_surface(
        const Heightfield& heightfield,
        float worldX,
        float worldZ) noexcept
    {
        if (!heightfield.valid() ||
            !std::isfinite(worldX) ||
            !std::isfinite(worldZ))
        {
            return std::nullopt;
        }

        const HeightfieldDescriptor& descriptor = heightfield.descriptor;
        const float localX =
            (worldX - descriptor.origin.x) / descriptor.sampleSpacing;
        const float localZ =
            (worldZ - descriptor.origin.z) / descriptor.sampleSpacing;
        const float maximumX =
            static_cast<float>(descriptor.widthSamples - 1);
        const float maximumZ =
            static_cast<float>(descriptor.depthSamples - 1);
        if (localX < 0.0f || localZ < 0.0f ||
            localX > maximumX || localZ > maximumZ)
        {
            return std::nullopt;
        }

        const auto x0 = static_cast<std::uint32_t>(std::floor(localX));
        const auto z0 = static_cast<std::uint32_t>(std::floor(localZ));
        const std::uint32_t x1 =
            (std::min)(x0 + 1, descriptor.widthSamples - 1);
        const std::uint32_t z1 =
            (std::min)(z0 + 1, descriptor.depthSamples - 1);
        const float tx = localX - static_cast<float>(x0);
        const float tz = localZ - static_cast<float>(z0);

        const auto heightAt =
            [&](std::uint32_t x, std::uint32_t z) noexcept
            {
                return detail::world_height(
                    descriptor,
                    heightfield.normalizedSamples[
                        detail::sample_index(descriptor, x, z)]);
            };

        const float h00 = heightAt(x0, z0);
        const float h10 = heightAt(x1, z0);
        const float h01 = heightAt(x0, z1);
        const float h11 = heightAt(x1, z1);
        const float heightNear = h00 + (h10 - h00) * tx;
        const float heightFar = h01 + (h11 - h01) * tx;
        const float height = heightNear + (heightFar - heightNear) * tz;

        const float left = heightAt(x0 > 0 ? x0 - 1 : x0, z0);
        const float right = heightAt(x1, z0);
        const float back = heightAt(x0, z0 > 0 ? z0 - 1 : z0);
        const float forward = heightAt(x0, z1);
        const Float3 normal = epochengine::render_math::normalize(
            {
                left - right,
                2.0f * descriptor.sampleSpacing,
                back - forward
            },
            {0.0f, 1.0f, 0.0f});

        return TerrainSurfaceSample{
            .position = {
                worldX,
                descriptor.origin.y + height,
                worldZ
            },
            .normal = normal,
            .materialSlot = descriptor.materialSlot
        };
    }

    struct TerrainMeshPlan final
    {
        TerrainAssetId asset{};
        std::uint64_t revision{};
        std::uint64_t contentHash{};
        std::uint64_t vertexCount{};
        std::uint64_t triangleCount{};
        std::uint64_t indexCount{};
        TerrainBounds bounds{};
        bool requiresNativeUpload{true};
    };

    [[nodiscard]] inline std::optional<TerrainMeshPlan> make_mesh_plan(
        const Heightfield& heightfield) noexcept
    {
        if (!heightfield.valid())
        {
            return std::nullopt;
        }

        const std::uint64_t width = heightfield.descriptor.widthSamples;
        const std::uint64_t depth = heightfield.descriptor.depthSamples;
        const std::uint64_t cellCount = (width - 1) * (depth - 1);
        return TerrainMeshPlan{
            .asset = heightfield.descriptor.asset,
            .revision = heightfield.revision,
            .contentHash = heightfield.contentHash,
            .vertexCount = width * depth,
            .triangleCount = cellCount * 2,
            .indexCount = cellCount * 6,
            .bounds = heightfield.bounds,
            .requiresNativeUpload = true
        };
    }
}
