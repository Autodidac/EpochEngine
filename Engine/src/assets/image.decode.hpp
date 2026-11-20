// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace epochengine::image_codec
{
    enum class PngStatus : std::uint8_t { ready, malformed, dimensions_exceeded, allocation_failure };
    struct PngResult
    {
        PngStatus status{PngStatus::malformed};
        std::uint32_t width{}, height{};
        std::vector<std::byte> rgba{};
    };
    // Bounded PNG decode; top-left RGBA8, straight alpha, no implicit gamma conversion.
    // The caller declares linear/sRGB intent when compiling its texture artifact.
    [[nodiscard]] PngResult decode_png(std::span<const std::byte> source,
        std::uint32_t maximum_dimension, std::uint64_t maximum_decoded_bytes) noexcept;
    [[nodiscard]] std::vector<std::byte> encode_rgba_png(std::span<const std::byte> rgba,
        std::uint32_t width, std::uint32_t height) noexcept;
    [[nodiscard]] bool make_magenta_cutout(std::span<std::byte> rgba,
        std::uint32_t width, std::uint32_t height) noexcept;
}
