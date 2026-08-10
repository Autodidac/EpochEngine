/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

export module render.canvas2d_evidence;

import render.canvas2d;
import render.canvas2d_cpu;

export namespace epochengine::canvas2d::evidence
{
    enum class PixelOrigin : std::uint8_t
    {
        top_left,
        bottom_left
    };

    [[nodiscard]] constexpr bool valid(PixelOrigin origin) noexcept
    {
        return origin == PixelOrigin::top_left
            || origin == PixelOrigin::bottom_left;
    }

    struct PixelReadbackView final
    {
        CanvasExtent extent{};
        std::uint32_t row_stride_pixels{};
        std::span<const cpu::Rgba8> pixels{};
        PixelOrigin origin{PixelOrigin::top_left};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            if (extent.empty() || row_stride_pixels < extent.width
                || !evidence::valid(origin))
            {
                return false;
            }
            const std::uint64_t required =
                static_cast<std::uint64_t>(extent.height - 1u)
                    * row_stride_pixels
                + extent.width;
            return required <= pixels.size();
        }
    };

    struct PixelEvidencePolicy final
    {
        std::uint64_t maximum_pixels{33'554'432};
        std::uint64_t maximum_outlier_pixels{};
        std::uint8_t channel_tolerance{1};
        bool compare_alpha{true};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_pixels != 0
                && maximum_outlier_pixels <= maximum_pixels;
        }
    };

    enum class PixelEvidenceCode : std::uint8_t
    {
        match,
        mismatch,
        invalid_reference,
        invalid_observation,
        invalid_policy,
        extent_mismatch,
        capacity_exceeded,
        readback_unavailable,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view pixel_evidence_code_name(
        PixelEvidenceCode code) noexcept
    {
        switch (code)
        {
        case PixelEvidenceCode::match: return "match";
        case PixelEvidenceCode::mismatch: return "mismatch";
        case PixelEvidenceCode::invalid_reference: return "invalid_reference";
        case PixelEvidenceCode::invalid_observation: return "invalid_observation";
        case PixelEvidenceCode::invalid_policy: return "invalid_policy";
        case PixelEvidenceCode::extent_mismatch: return "extent_mismatch";
        case PixelEvidenceCode::capacity_exceeded: return "capacity_exceeded";
        case PixelEvidenceCode::readback_unavailable:
            return "readback_unavailable";
        case PixelEvidenceCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    inline constexpr std::uint32_t invalid_coordinate =
        (std::numeric_limits<std::uint32_t>::max)();

    struct PixelEvidenceResult final
    {
        PixelEvidenceCode code{PixelEvidenceCode::invalid_reference};
        CanvasExtent extent{};
        std::uint64_t reference_hash{};
        std::uint64_t observed_hash{};
        std::uint64_t pixels_compared{};
        std::uint64_t channels_compared{};
        std::uint64_t nonidentical_pixels{};
        std::uint64_t outlier_pixels{};
        std::uint64_t outlier_channels{};
        std::uint64_t absolute_error_sum{};
        std::uint64_t mean_absolute_error_millionths{};
        std::uint8_t maximum_channel_error{};
        std::uint32_t first_outlier_x{invalid_coordinate};
        std::uint32_t first_outlier_y{invalid_coordinate};

        [[nodiscard]] constexpr bool complete() const noexcept
        {
            return code == PixelEvidenceCode::match
                || code == PixelEvidenceCode::mismatch;
        }

        [[nodiscard]] constexpr bool matched() const noexcept
        {
            return code == PixelEvidenceCode::match;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return complete();
        }
    };

    [[nodiscard]] std::uint64_t canonical_image_hash(
        const cpu::Image& image) noexcept;

    [[nodiscard]] PixelEvidenceResult compare_pixels(
        const cpu::Image& reference,
        std::uint64_t referenceHash,
        PixelReadbackView observed,
        const PixelEvidencePolicy& policy = {}) noexcept;

    enum class PixelEvidenceContractFailure : std::uint8_t
    {
        none,
        invalid_reference,
        invalid_observation,
        extent_mismatch,
        capacity,
        top_left_match,
        bottom_left_match,
        padded_stride,
        mismatch_metrics,
        tolerance,
        alpha_policy,
        hash_validation
    };

    [[nodiscard]] constexpr std::string_view
        pixel_evidence_contract_failure_name(
            PixelEvidenceContractFailure failure) noexcept
    {
        switch (failure)
        {
        case PixelEvidenceContractFailure::none: return "pass";
        case PixelEvidenceContractFailure::invalid_reference:
            return "invalid_reference";
        case PixelEvidenceContractFailure::invalid_observation:
            return "invalid_observation";
        case PixelEvidenceContractFailure::extent_mismatch:
            return "extent_mismatch";
        case PixelEvidenceContractFailure::capacity: return "capacity";
        case PixelEvidenceContractFailure::top_left_match:
            return "top_left_match";
        case PixelEvidenceContractFailure::bottom_left_match:
            return "bottom_left_match";
        case PixelEvidenceContractFailure::padded_stride:
            return "padded_stride";
        case PixelEvidenceContractFailure::mismatch_metrics:
            return "mismatch_metrics";
        case PixelEvidenceContractFailure::tolerance: return "tolerance";
        case PixelEvidenceContractFailure::alpha_policy: return "alpha_policy";
        case PixelEvidenceContractFailure::hash_validation:
            return "hash_validation";
        }
        return "unknown";
    }

    [[nodiscard]] PixelEvidenceContractFailure
        canvas2d_pixel_evidence_runtime_contract_failure() noexcept;
}
