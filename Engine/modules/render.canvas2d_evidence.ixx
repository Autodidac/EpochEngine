/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;
#include <cstddef>

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

    struct NativeReadbackRegion final
    {
        CanvasExtent framebuffer_extent{};
        RectI viewport{};

        [[nodiscard]] constexpr bool valid_for(
            CanvasExtent output) const noexcept
        {
            return !framebuffer_extent.empty()
                && !output.empty()
                && !viewport.empty()
                && viewport.x >= 0
                && viewport.y >= 0
                && viewport.width == output.width
                && viewport.height == output.height
                && static_cast<std::uint64_t>(viewport.x)
                    + viewport.width <= framebuffer_extent.width
                && static_cast<std::uint64_t>(viewport.y)
                    + viewport.height <= framebuffer_extent.height;
        }
    };

    struct NativeReadbackLayout final
    {
        std::uint32_t row_stride_pixels{};
        PixelOrigin origin{PixelOrigin::top_left};

        [[nodiscard]] constexpr bool valid_for(
            CanvasExtent extent) const noexcept
        {
            return !extent.empty()
                && row_stride_pixels >= extent.width
                && evidence::valid(origin);
        }

        [[nodiscard]] constexpr std::uint64_t required_pixels(
            CanvasExtent extent) const noexcept
        {
            return valid_for(extent)
                ? static_cast<std::uint64_t>(extent.height - 1u)
                    * row_stride_pixels + extent.width
                : 0u;
        }
    };

    struct NativeReadbackRequest final
    {
        NativeReadbackRegion region{};
        NativeReadbackLayout layout{};
        std::span<cpu::Rgba8> destination{};
    };
    enum class NativeChannelOrder : std::uint8_t
    {
        rgba,
        bgra
    };

    [[nodiscard]] constexpr bool valid(NativeChannelOrder order) noexcept
    {
        return order == NativeChannelOrder::rgba
            || order == NativeChannelOrder::bgra;
    }

    struct MappedNativeRows final
    {
        CanvasExtent extent{};
        std::uint64_t row_pitch_bytes{};
        std::span<const std::byte> bytes{};
        PixelOrigin origin{PixelOrigin::top_left};
        NativeChannelOrder channels{NativeChannelOrder::rgba};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            if (extent.empty() || !evidence::valid(origin)
                || !evidence::valid(channels))
            {
                return false;
            }
            const std::uint64_t tightRow =
                static_cast<std::uint64_t>(extent.width) * 4u;
            if (row_pitch_bytes < tightRow)
                return false;
            const std::uint64_t required =
                static_cast<std::uint64_t>(extent.height - 1u)
                    * row_pitch_bytes
                + tightRow;
            return required <= bytes.size();
        }
    };

    [[nodiscard]] bool copy_mapped_rgba8(
        MappedNativeRows source,
        NativeReadbackLayout destinationLayout,
        std::span<cpu::Rgba8> destination) noexcept;

    using DescribeNativeReadback = NativeReadbackLayout (*)(
        void*, const NativeReadbackRegion&) noexcept;
    using ReadNativePixels = bool (*)(
        void*, const NativeReadbackRequest&) noexcept;

    struct NativeReadbackHooks final
    {
        void* user{};
        DescribeNativeReadback describe{};
        ReadNativePixels read{};

        [[nodiscard]] constexpr bool ready() const noexcept
        {
            return describe != nullptr && read != nullptr;
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

    [[nodiscard]] PixelEvidenceResult compare_native_pixels(
        const cpu::Image& reference,
        std::uint64_t referenceHash,
        NativeReadbackRegion region,
        NativeReadbackHooks hooks,
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
        hash_validation,
        missing_native_hook,
        native_capacity,
        native_refusal,
        native_origin,
        native_stride,
        mapped_invalid,
        mapped_capacity,
        mapped_row_pitch,
        mapped_bgra
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
        case PixelEvidenceContractFailure::missing_native_hook:
            return "missing_native_hook";
        case PixelEvidenceContractFailure::native_capacity:
            return "native_capacity";
        case PixelEvidenceContractFailure::native_refusal:
            return "native_refusal";
        case PixelEvidenceContractFailure::native_origin:
            return "native_origin";
        case PixelEvidenceContractFailure::native_stride:
        case PixelEvidenceContractFailure::mapped_invalid: return "mapped_invalid";
        case PixelEvidenceContractFailure::mapped_capacity: return "mapped_capacity";
        case PixelEvidenceContractFailure::mapped_row_pitch: return "mapped_row_pitch";
        case PixelEvidenceContractFailure::mapped_bgra: return "mapped_bgra";
            return "native_stride";
        }
        return "unknown";
    }

    [[nodiscard]] PixelEvidenceContractFailure
        canvas2d_pixel_evidence_runtime_contract_failure() noexcept;
}
