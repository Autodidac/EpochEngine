/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#pragma once

#include <cstdint>
#include <memory>

namespace epochengine::core
{
    class Context;
    struct RenderViewport;
}

namespace epochengine::openglcanvas2d
{
    enum class Canvas2DPixelEvidenceState : std::uint8_t
    {
        not_requested,
        warming,
        match,
        mismatch,
        unavailable
    };

    struct Canvas2DPixelEvidenceSnapshot final
    {
        Canvas2DPixelEvidenceState state{
            Canvas2DPixelEvidenceState::not_requested};
        std::uint64_t frame_sequence{};
        std::uint64_t content_hash{};
        std::uint64_t reference_hash{};
        std::uint64_t observed_hash{};
        std::uint64_t pixels_compared{};
        std::uint64_t nonidentical_pixels{};
        std::uint64_t outlier_pixels{};
        std::uint64_t mean_absolute_error_millionths{};
        std::uint8_t maximum_channel_error{};
        std::uint32_t first_outlier_x{0xffffffffu};
        std::uint32_t first_outlier_y{0xffffffffu};
    };

    [[nodiscard]] bool render_canvas2d_scene_content(
        const std::shared_ptr<core::Context>& ctx,
        const core::RenderViewport& viewport,
        int framebuffer_width,
        int framebuffer_height);

    [[nodiscard]] Canvas2DPixelEvidenceSnapshot
        canvas2d_scene_pixel_evidence(const core::Context* ctx) noexcept;

    void release_canvas2d_scene_renderer(const core::Context* ctx) noexcept;
}
