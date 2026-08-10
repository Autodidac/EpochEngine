/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

module render.canvas2d_evidence;

import render.device;

namespace epochengine::canvas2d::evidence
{
    namespace
    {
        inline constexpr std::uint64_t kHashOffset = 1469598103934665603ull;
        inline constexpr std::uint64_t kHashPrime = 1099511628211ull;

        constexpr void hash_byte(
            std::uint64_t& value,
            std::uint8_t byte) noexcept
        {
            value ^= byte;
            value *= kHashPrime;
        }

        template <typename Integer>
        constexpr void hash_integer(
            std::uint64_t& value,
            Integer integer) noexcept
        {
            using Unsigned = std::make_unsigned_t<Integer>;
            const Unsigned bits = static_cast<Unsigned>(integer);
            for (std::size_t byte = 0; byte < sizeof(Unsigned); ++byte)
            {
                hash_byte(
                    value,
                    static_cast<std::uint8_t>(bits >> (byte * 8u)));
            }
        }

        [[nodiscard]] constexpr const cpu::Rgba8* observed_row(
            PixelReadbackView observed,
            std::uint32_t logicalY) noexcept
        {
            const std::uint32_t storageY =
                observed.origin == PixelOrigin::top_left
                ? logicalY
                : observed.extent.height - logicalY - 1u;
            return observed.pixels.data()
                + static_cast<std::size_t>(storageY)
                    * observed.row_stride_pixels;
        }

        [[nodiscard]] std::uint64_t observed_hash(
            PixelReadbackView observed) noexcept
        {
            if (!observed.valid())
                return 0;
            std::uint64_t value = kHashOffset;
            hash_byte(value, 1);
            hash_integer(value, observed.extent.width);
            hash_integer(value, observed.extent.height);
            hash_integer(
                value,
                static_cast<std::uint64_t>(observed.extent.width)
                    * sizeof(cpu::Rgba8));
            hash_byte(
                value,
                static_cast<std::uint8_t>(TextureFormat::rgba8_unorm));
            for (std::uint32_t y = 0; y < observed.extent.height; ++y)
            {
                const cpu::Rgba8* const row = observed_row(observed, y);
                for (std::uint32_t x = 0; x < observed.extent.width; ++x)
                {
                    const cpu::Rgba8 pixel = row[x];
                    hash_byte(value, pixel.r);
                    hash_byte(value, pixel.g);
                    hash_byte(value, pixel.b);
                    hash_byte(value, pixel.a);
                }
            }
            return value == 0 ? 1 : value;
        }

        [[nodiscard]] constexpr std::uint8_t channel_error(
            std::uint8_t left,
            std::uint8_t right) noexcept
        {
            return left > right
                ? static_cast<std::uint8_t>(left - right)
                : static_cast<std::uint8_t>(right - left);
        }
    }

    std::uint64_t canonical_image_hash(const cpu::Image& image) noexcept
    {
        if (!image.valid())
            return 0;
        return observed_hash(PixelReadbackView{
            image.extent,
            image.extent.width,
            image.pixels,
            PixelOrigin::top_left});
    }

    PixelEvidenceResult compare_pixels(
        const cpu::Image& reference,
        std::uint64_t referenceHash,
        PixelReadbackView observed,
        const PixelEvidencePolicy& policy) noexcept
    {
        PixelEvidenceResult result{};
        result.extent = reference.extent;
        if (!reference.valid())
            return result;
        result.reference_hash = canonical_image_hash(reference);
        if (result.reference_hash == 0
            || (referenceHash != 0 && referenceHash != result.reference_hash))
        {
            return result;
        }
        if (!policy.valid())
        {
            result.code = PixelEvidenceCode::invalid_policy;
            return result;
        }
        if (!observed.valid())
        {
            result.code = PixelEvidenceCode::invalid_observation;
            return result;
        }
        if (observed.extent != reference.extent)
        {
            result.code = PixelEvidenceCode::extent_mismatch;
            return result;
        }

        const std::uint64_t pixelCount =
            static_cast<std::uint64_t>(reference.extent.width)
                * reference.extent.height;
        if (pixelCount == 0 || pixelCount > policy.maximum_pixels)
        {
            result.code = PixelEvidenceCode::capacity_exceeded;
            return result;
        }

        result.observed_hash = observed_hash(observed);
        result.pixels_compared = pixelCount;
        const std::uint64_t channelsPerPixel = policy.compare_alpha ? 4u : 3u;
        result.channels_compared = pixelCount * channelsPerPixel;
        for (std::uint32_t y = 0; y < reference.extent.height; ++y)
        {
            const cpu::Rgba8* const expectedRow = reference.row(y);
            const cpu::Rgba8* const observedRow = observed_row(observed, y);
            for (std::uint32_t x = 0; x < reference.extent.width; ++x)
            {
                const cpu::Rgba8 expected = expectedRow[x];
                const cpu::Rgba8 actual = observedRow[x];
                const std::array errors{
                    channel_error(expected.r, actual.r),
                    channel_error(expected.g, actual.g),
                    channel_error(expected.b, actual.b),
                    channel_error(expected.a, actual.a)};
                bool nonidentical{};
                bool outlier{};
                const std::size_t comparedChannels = policy.compare_alpha ? 4u : 3u;
                for (std::size_t channel = 0;
                    channel < comparedChannels;
                    ++channel)
                {
                    const std::uint8_t error = errors[channel];
                    result.absolute_error_sum += error;
                    result.maximum_channel_error = (std::max)(
                        result.maximum_channel_error, error);
                    nonidentical = nonidentical || error != 0;
                    if (error > policy.channel_tolerance)
                    {
                        outlier = true;
                        ++result.outlier_channels;
                    }
                }
                if (nonidentical)
                    ++result.nonidentical_pixels;
                if (!outlier)
                    continue;
                ++result.outlier_pixels;
                if (result.first_outlier_x == invalid_coordinate)
                {
                    result.first_outlier_x = x;
                    result.first_outlier_y = y;
                }
            }
        }

        if (result.channels_compared != 0)
        {
            const std::uint64_t scale = 1'000'000;
            if (result.absolute_error_sum
                <= (std::numeric_limits<std::uint64_t>::max)() / scale)
            {
                result.mean_absolute_error_millionths =
                    result.absolute_error_sum * scale
                    / result.channels_compared;
            }
            else
            {
                result.mean_absolute_error_millionths =
                    (result.absolute_error_sum / result.channels_compared)
                    * scale;
            }
        }
        result.code = result.outlier_pixels <= policy.maximum_outlier_pixels
            ? PixelEvidenceCode::match
            : PixelEvidenceCode::mismatch;
        return result;
    }

    PixelEvidenceContractFailure
        canvas2d_pixel_evidence_runtime_contract_failure() noexcept
    {
        cpu::Image reference{};
        reference.extent = {2, 2};
        reference.pixels = {
            {10, 20, 30, 40},
            {50, 60, 70, 80},
            {90, 100, 110, 120},
            {130, 140, 150, 160}};
        const std::uint64_t referenceHash = canonical_image_hash(reference);
        if (referenceHash == 0)
            return PixelEvidenceContractFailure::invalid_reference;
        if (compare_pixels({}, 0, {}).code
            != PixelEvidenceCode::invalid_reference)
        {
            return PixelEvidenceContractFailure::invalid_reference;
        }
        if (compare_pixels(reference, referenceHash, {}).code
            != PixelEvidenceCode::invalid_observation)
        {
            return PixelEvidenceContractFailure::invalid_observation;
        }

        const PixelReadbackView topLeft{
            reference.extent,
            reference.extent.width,
            reference.pixels,
            PixelOrigin::top_left};
        const PixelEvidenceResult exact = compare_pixels(
            reference,
            referenceHash,
            topLeft,
            PixelEvidencePolicy{.channel_tolerance = 0});
        if (!exact.matched() || exact.reference_hash != exact.observed_hash
            || exact.nonidentical_pixels != 0 || exact.outlier_pixels != 0)
        {
            return PixelEvidenceContractFailure::top_left_match;
        }

        const std::array bottomLeft{
            reference.pixels[2], reference.pixels[3],
            reference.pixels[0], reference.pixels[1]};
        const PixelEvidenceResult flipped = compare_pixels(
            reference,
            referenceHash,
            PixelReadbackView{
                reference.extent, 2, bottomLeft, PixelOrigin::bottom_left},
            PixelEvidencePolicy{.channel_tolerance = 0});
        if (!flipped.matched() || flipped.observed_hash != referenceHash)
            return PixelEvidenceContractFailure::bottom_left_match;

        const std::array padded{
            reference.pixels[0], reference.pixels[1], cpu::Rgba8{},
            reference.pixels[2], reference.pixels[3], cpu::Rgba8{}};
        if (!compare_pixels(
                reference,
                referenceHash,
                PixelReadbackView{
                    reference.extent, 3, padded, PixelOrigin::top_left},
                PixelEvidencePolicy{.channel_tolerance = 0}).matched())
        {
            return PixelEvidenceContractFailure::padded_stride;
        }

        cpu::Image wrongExtent = reference;
        wrongExtent.extent = {1, 4};
        if (compare_pixels(
                reference,
                referenceHash,
                PixelReadbackView{
                    wrongExtent.extent,
                    wrongExtent.extent.width,
                    wrongExtent.pixels,
                    PixelOrigin::top_left}).code
            != PixelEvidenceCode::extent_mismatch)
        {
            return PixelEvidenceContractFailure::extent_mismatch;
        }
        PixelEvidencePolicy capacity{};
        capacity.maximum_pixels = 3;
        if (compare_pixels(reference, referenceHash, topLeft, capacity).code
            != PixelEvidenceCode::capacity_exceeded)
        {
            return PixelEvidenceContractFailure::capacity;
        }

        auto changed = reference.pixels;
        changed[1].g = static_cast<std::uint8_t>(changed[1].g + 4u);
        const PixelEvidenceResult mismatch = compare_pixels(
            reference,
            referenceHash,
            PixelReadbackView{
                reference.extent, 2, changed, PixelOrigin::top_left},
            PixelEvidencePolicy{.channel_tolerance = 1});
        if (mismatch.code != PixelEvidenceCode::mismatch
            || mismatch.nonidentical_pixels != 1
            || mismatch.outlier_pixels != 1
            || mismatch.outlier_channels != 1
            || mismatch.maximum_channel_error != 4
            || mismatch.first_outlier_x != 1 || mismatch.first_outlier_y != 0)
        {
            return PixelEvidenceContractFailure::mismatch_metrics;
        }
        PixelEvidencePolicy tolerant{};
        tolerant.channel_tolerance = 4;
        if (!compare_pixels(reference, referenceHash,
                PixelReadbackView{
                    reference.extent, 2, changed, PixelOrigin::top_left},
                tolerant).matched())
        {
            return PixelEvidenceContractFailure::tolerance;
        }

        changed = reference.pixels;
        changed[0].a = 0;
        PixelEvidencePolicy ignoreAlpha{};
        ignoreAlpha.channel_tolerance = 0;
        ignoreAlpha.compare_alpha = false;
        if (!compare_pixels(reference, referenceHash,
                PixelReadbackView{
                    reference.extent, 2, changed, PixelOrigin::top_left},
                ignoreAlpha).matched())
        {
            return PixelEvidenceContractFailure::alpha_policy;
        }
        if (compare_pixels(reference, referenceHash ^ 1u, topLeft).code
            != PixelEvidenceCode::invalid_reference)
        {
            return PixelEvidenceContractFailure::hash_validation;
        }
        return PixelEvidenceContractFailure::none;
    }
}
