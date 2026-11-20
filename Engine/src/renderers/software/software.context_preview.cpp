module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <span>

#include <include/engine.config.hpp>

module software.context;

import core.context;
import software.state;
import package.registry;
import render.arcade;
import render.camera;
import render.canvas2d_cpu;
import render.canvas2d_limits;
import render.canvas2d_runtime;
import render.context_frame;
import render.device;
import render.preview_grid;

namespace epochengine::anativecontext::detail
{
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
    struct Canvas2DBlitRegion final
    {
        int destinationX{};
        int destinationY{};
        int sourceX{};
        int sourceY{};
        int width{};
        int height{};

        [[nodiscard]] constexpr bool visible() const noexcept
        {
            return width > 0 && height > 0;
        }
    };

    [[nodiscard]] constexpr Canvas2DBlitRegion canvas2d_blit_region(
        int framebufferWidth,
        int framebufferHeight,
        const core::RenderViewport& viewport,
        int sourceWidth,
        int sourceHeight) noexcept
    {
        if (framebufferWidth <= 0 || framebufferHeight <= 0
            || !viewport.valid() || sourceWidth != viewport.width
            || sourceHeight != viewport.height)
        {
            return {};
        }

        const std::int64_t viewportRight = static_cast<std::int64_t>(viewport.x)
            + static_cast<std::int64_t>(viewport.width);
        const std::int64_t viewportBottom = static_cast<std::int64_t>(viewport.y)
            + static_cast<std::int64_t>(viewport.height);
        const std::int64_t clippedLeft = (std::max)(
            std::int64_t{0}, static_cast<std::int64_t>(viewport.x));
        const std::int64_t clippedTop = (std::max)(
            std::int64_t{0}, static_cast<std::int64_t>(viewport.y));
        const std::int64_t clippedRight = (std::min)(
            static_cast<std::int64_t>(framebufferWidth), viewportRight);
        const std::int64_t clippedBottom = (std::min)(
            static_cast<std::int64_t>(framebufferHeight), viewportBottom);
        if (clippedLeft >= clippedRight || clippedTop >= clippedBottom)
            return {};

        return {
            static_cast<int>(clippedLeft),
            static_cast<int>(clippedTop),
            static_cast<int>(clippedLeft - viewport.x),
            static_cast<int>(clippedTop - viewport.y),
            static_cast<int>(clippedRight - clippedLeft),
            static_cast<int>(clippedBottom - clippedTop)};
    }

    [[nodiscard]] constexpr std::uint32_t pack_canvas2d_pixel(
        canvas2d::cpu::Rgba8 pixel) noexcept
    {
        return (static_cast<std::uint32_t>(pixel.a) << 24u)
            | (static_cast<std::uint32_t>(pixel.r) << 16u)
            | (static_cast<std::uint32_t>(pixel.g) << 8u)
            | static_cast<std::uint32_t>(pixel.b);
    }

    [[nodiscard]] constexpr std::uint32_t blend_canvas2d_pixel(
        canvas2d::cpu::Rgba8 source,
        std::uint32_t destination) noexcept
    {
        const std::uint32_t inverseAlpha = 255u - source.a;
        const auto source_over = [inverseAlpha](
            std::uint32_t sourceChannel,
            std::uint32_t destinationChannel) noexcept
        {
            return (std::min)(
                255u,
                sourceChannel
                    + (destinationChannel * inverseAlpha + 127u) / 255u);
        };
        const std::uint32_t destinationAlpha = destination >> 24u;
        const std::uint32_t destinationRed = (destination >> 16u) & 0xffu;
        const std::uint32_t destinationGreen = (destination >> 8u) & 0xffu;
        const std::uint32_t destinationBlue = destination & 0xffu;
        return (source_over(source.a, destinationAlpha) << 24u)
            | (source_over(source.r, destinationRed) << 16u)
            | (source_over(source.g, destinationGreen) << 8u)
            | source_over(source.b, destinationBlue);
    }

    static_assert(
        blend_canvas2d_pixel({0u, 0u, 0u, 0u}, 0x7f123456u)
            == 0x7f123456u);
    static_assert(
        blend_canvas2d_pixel({0x12u, 0x34u, 0x56u, 0xffu}, 0x7fabcdefu)
            == 0xff123456u);
    static_assert(
        pack_canvas2d_pixel({0x12u, 0x34u, 0x56u, 0x78u}) == 0x78123456u);
    static_assert([]
    {
        constexpr core::RenderViewport viewport{-2, 1, 5, 4};
        constexpr Canvas2DBlitRegion region = canvas2d_blit_region(
            4, 3, viewport, 5, 4);
        return region.destinationX == 0 && region.destinationY == 1
            && region.sourceX == 2 && region.sourceY == 0
            && region.width == 3 && region.height == 2;
    }());

    [[nodiscard]] constexpr bool blit_canvas2d_surface(
        std::span<std::uint32_t> framebuffer,
        int framebufferWidth,
        int framebufferHeight,
        std::span<const canvas2d::cpu::Rgba8> source,
        int sourceWidth,
        int sourceHeight,
        const core::RenderViewport& viewport) noexcept
    {
        if (framebufferWidth <= 0 || framebufferHeight <= 0
            || sourceWidth <= 0 || sourceHeight <= 0)
        {
            return false;
        }
        const std::uint64_t requiredFramebuffer =
            static_cast<std::uint64_t>(framebufferWidth)
            * static_cast<std::uint64_t>(framebufferHeight);
        const std::uint64_t requiredSource =
            static_cast<std::uint64_t>(sourceWidth)
            * static_cast<std::uint64_t>(sourceHeight);
        if (requiredFramebuffer != framebuffer.size()
            || requiredSource != source.size())
        {
            return false;
        }

        const Canvas2DBlitRegion region = canvas2d_blit_region(
            framebufferWidth,
            framebufferHeight,
            viewport,
            sourceWidth,
            sourceHeight);
        if (!region.visible())
            return true;

        for (int row = 0; row < region.height; ++row)
        {
            const std::size_t destinationOffset =
                static_cast<std::size_t>(region.destinationY + row)
                    * static_cast<std::size_t>(framebufferWidth)
                + static_cast<std::size_t>(region.destinationX);
            const std::size_t sourceOffset =
                static_cast<std::size_t>(region.sourceY + row)
                    * static_cast<std::size_t>(sourceWidth)
                + static_cast<std::size_t>(region.sourceX);
            for (int column = 0; column < region.width; ++column)
            {
                std::uint32_t& destination =
                    framebuffer[destinationOffset + static_cast<std::size_t>(column)];
                destination = blend_canvas2d_pixel(
                    source[sourceOffset + static_cast<std::size_t>(column)],
                    destination);
            }
        }
        return true;
    }

    static_assert([]
    {
        constexpr std::uint32_t untouched = 0xAABBCCDDu;
        std::array<std::uint32_t, 15> framebuffer{};
        framebuffer.fill(untouched);
        constexpr std::array<canvas2d::cpu::Rgba8, 4> source{{
            {0x01u, 0x02u, 0x03u, 0x04u},
            {0x11u, 0x22u, 0x33u, 0x44u},
            {0x55u, 0x66u, 0x77u, 0x88u},
            {0x99u, 0xAAu, 0xBBu, 0xCCu}
        }};
        constexpr core::RenderViewport viewport{3, 1, 2, 2};
        return blit_canvas2d_surface(
                framebuffer, 5, 3, source, 2, 2, viewport)
            && framebuffer[8] == blend_canvas2d_pixel(source[0], untouched)
            && framebuffer[9] == blend_canvas2d_pixel(source[1], untouched)
            && framebuffer[13] == blend_canvas2d_pixel(source[2], untouched)
            && framebuffer[14] == blend_canvas2d_pixel(source[3], untouched)
            && framebuffer[7] == untouched
            && (framebuffer[13] >> 24u) >= source[2].a;
    }());

    class SoftwareCanvas2DPresenter final
    {
    public:

        [[nodiscard]] bool render(
            const core::Context& ctx,
            const core::RenderViewport& viewport,
            std::span<std::uint32_t> framebuffer,
            int framebufferWidth,
            int framebufferHeight) noexcept
        {
            std::scoped_lock lock(mutex_);
            if (!viewport.valid())
                return false;

            const canvas2d::runtime::PreparedSceneView prepared =
                session_.prepare(
                    &ctx,
                    {
                        static_cast<std::uint32_t>(viewport.width),
                        static_cast<std::uint32_t>(viewport.height)},
                    execution_limits_.canvas,
                    execution_limits_.raster);
            if (!prepared)
            {
                if (prepared.code != canvas2d::runtime::PrepareCode::missing_scene)
                    session_.reset();
                return false;
            }

            const auto& presentation = prepared.raster->presentation;
            return presentation.valid()
                && blit_canvas2d_surface(
                    framebuffer,
                    framebufferWidth,
                    framebufferHeight,
                    std::span<const canvas2d::cpu::Rgba8>{presentation.pixels},
                    static_cast<int>(presentation.extent.width),
                    static_cast<int>(presentation.extent.height),
                    viewport);
        }

        void reset() noexcept
        {
            std::scoped_lock lock(mutex_);
            session_.reset();
        }

    private:

        std::mutex mutex_{};
        canvas2d::limits::NativeExecutionLimits execution_limits_{
            canvas2d::limits::for_backend(RendererBackendKind::software)};
        canvas2d::runtime::SceneRasterSession session_{};
    };

    [[nodiscard]] SoftwareCanvas2DPresenter& canvas2d_presenter() noexcept
    {
        static SoftwareCanvas2DPresenter presenter{};
        return presenter;
    }


    void reset_canvas2d_scene_renderer() noexcept
    {
        canvas2d_presenter().reset();
    }

    [[nodiscard]] bool render_canvas2d_scene(
        const core::Context& ctx,
        const core::RenderViewport& viewport) noexcept
    {
        auto& sr = s_softrendererstate;
        return canvas2d_presenter().render(
            ctx,
            viewport,
            sr.framebuffer,
            sr.width,
            sr.height);
    }

    void refresh_dimensions(core::Context& ctx) noexcept
    {
        auto& sr = s_softrendererstate;
        ctx.width = (std::max)(1, sr.width);
        ctx.height = (std::max)(1, sr.height);
        ctx.virtualWidth = ctx.width;
        ctx.virtualHeight = ctx.height;
        ctx.framebufferWidth = ctx.width;
        ctx.framebufferHeight = ctx.height;

        if (ctx.windowData)
            ctx.windowData->set_size(ctx.width, ctx.height);
    }

    std::uint32_t pack_color(float r, float g, float b, float a) noexcept
    {
        const auto clamp_channel = [](float value) noexcept -> std::uint32_t
        {
            return static_cast<std::uint32_t>((std::clamp)(value, 0.0f, 1.0f) * 255.0f);
        };

        return (clamp_channel(a) << 24)
            | (clamp_channel(r) << 16)
            | (clamp_channel(g) << 8)
            | clamp_channel(b);
    }

    void fill_preview_rect(int x, int y, int width, int height, std::uint32_t color) noexcept
    {
        auto& sr = s_softrendererstate;
        if (sr.framebuffer.empty() || sr.width <= 0 || sr.height <= 0 || width <= 0 || height <= 0)
            return;

        const int x0 = (std::max)(0, x);
        const int y0 = (std::max)(0, y);
        const int x1 = (std::min)(sr.width, x + width);
        const int y1 = (std::min)(sr.height, y + height);
        if (x0 >= x1 || y0 >= y1)
            return;

        for (int py = y0; py < y1; ++py)
        {
            auto* rowBegin = sr.framebuffer.data()
                + static_cast<std::size_t>(py) * static_cast<std::size_t>(sr.width)
                + static_cast<std::size_t>(x0);
            std::fill_n(rowBegin, static_cast<std::size_t>(x1 - x0), color);
        }
    }

    void draw_line(
        int x0,
        int y0,
        int x1,
        int y1,
        std::uint32_t color,
        const core::RenderViewport& viewport) noexcept
    {
        auto& sr = s_softrendererstate;
        if (sr.framebuffer.empty() || sr.width <= 0 || sr.height <= 0)
            return;

        int dx = std::abs(x1 - x0);
        const int sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0);
        const int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        for (;;)
        {
            if (x0 >= viewport.x
                && x0 < viewport.x + viewport.width
                && y0 >= viewport.y
                && y0 < viewport.y + viewport.height
                && x0 >= 0
                && x0 < sr.width
                && y0 >= 0
                && y0 < sr.height)
            {
                sr.framebuffer[
                    static_cast<std::size_t>(y0) * static_cast<std::size_t>(sr.width)
                    + static_cast<std::size_t>(x0)] = color;
            }

            if (x0 == x1 && y0 == y1)
                break;

            const int twiceErr = err * 2;
            if (twiceErr >= dy)
            {
                err += dy;
                x0 += sx;
            }
            if (twiceErr <= dx)
            {
                err += dx;
                y0 += sy;
            }
        }
    }

    struct SoftwarePreviewVertex
    {
        float x{};
        float y{};
        float depth{};
    };

    enum class SoftwarePreviewRasterPass { solid, wire_depth, wire_color };

    template<SoftwarePreviewRasterPass pass = SoftwarePreviewRasterPass::solid>
    void draw_triangle(
        const SoftwarePreviewVertex& a,
        const SoftwarePreviewVertex& b,
        const SoftwarePreviewVertex& c,
        std::uint32_t color,
        const core::RenderViewport& viewport,
        unsigned wireEdges) noexcept
    {
        auto& sr = s_softrendererstate;
        if (sr.framebuffer.empty() || sr.width <= 0 || sr.height <= 0)
            return;

        const auto edge = [](float px, float py, float x0, float y0, float x1, float y1) noexcept
        {
            return ((px - x0) * (y1 - y0)) - ((py - y0) * (x1 - x0));
        };

        const float area = edge(a.x, a.y, b.x, b.y, c.x, c.y);
        if (!std::isfinite(area) || std::abs(area) <= 1.0e-4f)
            return;

        const float minX = (std::max)(static_cast<float>(viewport.x), (std::min)({a.x, b.x, c.x}));
        const int lastX = viewport.x + viewport.width - 1;
        const float maxX = (std::min)(static_cast<float>(lastX) + 1.0f, (std::max)({a.x, b.x, c.x}));
        const float minY = (std::max)(static_cast<float>(viewport.y), (std::min)({a.y, b.y, c.y}));
        const int lastY = viewport.y + viewport.height - 1;
        const float maxY = (std::min)(static_cast<float>(lastY) + 1.0f, (std::max)({a.y, b.y, c.y}));
        if (minX > maxX || minY > maxY)
            return;
        const int x0 = static_cast<int>(std::floor(minX));
        const int x1 = (std::min)(lastX, static_cast<int>(std::ceil(maxX)));
        const int y0 = static_cast<int>(std::floor(minY));
        const int y1 = (std::min)(lastY, static_cast<int>(std::ceil(maxY)));
        if (x0 > x1 || y0 > y1)
            return;

        const bool positive = area > 0.0f;
        const float inverseArea = 1.0f / area;
        std::array<float, 3> edgeWidth{};
        if constexpr (pass == SoftwarePreviewRasterPass::wire_color)
        {
            constexpr float pixelWidth = 1.25f;
            edgeWidth = {
                pixelWidth * std::hypot(c.x - b.x, c.y - b.y),
                pixelWidth * std::hypot(a.x - c.x, a.y - c.y),
                pixelWidth * std::hypot(b.x - a.x, b.y - a.y)
            };
        }
        for (int py = y0; py <= y1; ++py)
        {
            for (int px = x0; px <= x1; ++px)
            {
                const float sampleX = static_cast<float>(px) + 0.5f;
                const float sampleY = static_cast<float>(py) + 0.5f;
                const float w0 = edge(sampleX, sampleY, b.x, b.y, c.x, c.y);
                const float w1 = edge(sampleX, sampleY, c.x, c.y, a.x, a.y);
                const float w2 = edge(sampleX, sampleY, a.x, a.y, b.x, b.y);
                if (positive ? (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) : (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f))
                {
                    const float depth = (w0 * a.depth + w1 * b.depth + w2 * c.depth) * inverseArea;
                    const auto depthIndex = static_cast<std::size_t>(py - viewport.y) * viewport.width
                        + static_cast<std::size_t>(px - viewport.x);
                    auto& visibleDepth = sr.previewDepth[depthIndex];
                    if constexpr (pass == SoftwarePreviewRasterPass::wire_color)
                    {
                        if (depth != visibleDepth
                            || !(((wireEdges & 1u) && std::abs(w0) <= edgeWidth[0])
                                || ((wireEdges & 2u) && std::abs(w1) <= edgeWidth[1])
                                || ((wireEdges & 4u) && std::abs(w2) <= edgeWidth[2])))
                            continue;
                    }
                    else
                    {
                        if (!(depth < visibleDepth))
                            continue;
                        visibleDepth = depth;
                    }
                    if constexpr (pass != SoftwarePreviewRasterPass::wire_depth)
                        sr.framebuffer[static_cast<std::size_t>(py) * sr.width + static_cast<std::size_t>(px)] = color;
                }
            }
        }
    }

    bool prepare_preview_depth(const core::RenderViewport& viewport) noexcept
    {
        auto& sr = s_softrendererstate;
        const auto width = static_cast<std::size_t>(viewport.width);
        const auto height = static_cast<std::size_t>(viewport.height);
        if (!width || !height || width > sr.framebuffer.size() / height)
            return false;
        try
        {
            sr.previewDepth.assign(width * height, std::numeric_limits<float>::infinity());
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    template<SoftwarePreviewRasterPass pass = SoftwarePreviewRasterPass::solid>
    void draw_clipped_preview_triangle(std::array<previewgrid::ClipVertex, 3> triangle,
        std::uint32_t color, const core::RenderViewport& viewport,
        render_camera::ProjectionKind projection) noexcept
    {
        const auto polygon = previewgrid::clip_triangle<pass == SoftwarePreviewRasterPass::wire_color>(triangle);
        const auto count = polygon.count;
        const auto& originalEdges = polygon.originalEdges;
        std::array<SoftwarePreviewVertex, std::tuple_size_v<decltype(polygon.vertices)>> screen{};
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto& vertex = polygon.vertices[index];
            if (!(vertex.w > 0.0f))
                return;
            // Reciprocal camera depth is linear in screen space. It avoids the
            // precision lost to the constant offset in perspective clip z/w.
            const float depth = projection == render_camera::ProjectionKind::perspective
                ? -1.0f / vertex.w : (std::clamp)(vertex.z / vertex.w, -1.0f, 1.0f);
            screen[index] = {
                viewport.x + (0.5f + 0.5f * (vertex.x / vertex.w)) * viewport.width,
                viewport.y + (0.5f - 0.5f * (vertex.y / vertex.w)) * viewport.height,
                depth
            };
        }
        for (std::size_t index = 1; index + 1 < count; ++index)
        {
            unsigned wireEdges = 0;
            if constexpr (pass == SoftwarePreviewRasterPass::wire_color)
            {
                // A clipping edge or fan diagonal cannot acquire a wire color.
                wireEdges = ((originalEdges[index] & originalEdges[index + 1]) ? 1u : 0u)
                    | ((originalEdges[index + 1] & originalEdges[0]) ? 2u : 0u)
                    | ((originalEdges[0] & originalEdges[index]) ? 4u : 0u);
            }
            draw_triangle<pass>(screen[0], screen[index], screen[index + 1], color, viewport, wireEdges);
        }
    }

    struct SoftwareTextureVertex final
    {
        float x{};
        float y{};
        float u{};
        float v{};
    };

    [[nodiscard]] bool ensure_arcade_screen_surface() noexcept
    {
        constexpr int kMaximumDimension = 2048;
        constexpr std::size_t kMaximumPixels =
            static_cast<std::size_t>(kMaximumDimension) * kMaximumDimension;
        static_assert(package_registry::engine_arcade_render_texture_width() > 0u);
        static_assert(package_registry::engine_arcade_render_texture_height() > 0u);
        static_assert(
            package_registry::engine_arcade_render_texture_width() <= kMaximumDimension);
        static_assert(
            package_registry::engine_arcade_render_texture_height() <= kMaximumDimension);

        auto& target = s_softrendererstate.arcadeScreen;
        const int width = static_cast<int>(package_registry::engine_arcade_render_texture_width());
        const int height = static_cast<int>(package_registry::engine_arcade_render_texture_height());
        if (width <= 0 || height <= 0
            || width > kMaximumDimension || height > kMaximumDimension
            || static_cast<std::size_t>(width) * static_cast<std::size_t>(height) > kMaximumPixels)
        {
            return false;
        }
        if (target.ready() && target.width == width && target.height == height)
            return true;

        target = {};
        try
        {
            target.pixels.assign(
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
                0xff04060bu);
        }
        catch (...)
        {
            target = {};
            return false;
        }
        target.width = width;
        target.height = height;
        return true;
    }

    void fill_arcade_surface_rect(
        const render_arcade::ArcadePreviewRect& rect) noexcept
    {
        auto& target = s_softrendererstate.arcadeScreen;
        if (!target.ready() || rect.width <= 0 || rect.height <= 0)
            return;

        const int x0 = (std::clamp)(rect.x, 0, target.width);
        const int y0 = (std::clamp)(rect.y, 0, target.height);
        const int x1 = (std::clamp)(rect.x + rect.width, 0, target.width);
        const int y1 = (std::clamp)(rect.y + rect.height, 0, target.height);
        if (x0 >= x1 || y0 >= y1)
            return;

        const std::uint32_t color = pack_color(
            rect.color[0], rect.color[1], rect.color[2], rect.color[3]);
        for (int y = y0; y < y1; ++y)
        {
            auto* row = target.pixels.data()
                + static_cast<std::size_t>(y) * static_cast<std::size_t>(target.width)
                + static_cast<std::size_t>(x0);
            std::fill_n(row, static_cast<std::size_t>(x1 - x0), color);
        }
    }

    [[nodiscard]] bool prepare_arcade_screen_surface() noexcept
    {
        auto& target = s_softrendererstate.arcadeScreen;
        if (!ensure_arcade_screen_surface())
            return false;
        if (target.preparedThisFrame)
            return true;

        std::fill(target.pixels.begin(), target.pixels.end(), 0xff04060bu);
        render_arcade::emit_arcade_attract_pattern(
            target.width,
            target.height,
            target.frame++,
            [](const render_arcade::ArcadePreviewRect& rect) noexcept
            {
                fill_arcade_surface_rect(rect);
            });
        target.preparedThisFrame = true;
        return true;
    }

    void draw_textured_triangle(
        const SoftwareTextureVertex& a,
        const SoftwareTextureVertex& b,
        const SoftwareTextureVertex& c,
        const core::RenderViewport& viewport) noexcept
    {
        auto& sr = s_softrendererstate;
        const auto& target = sr.arcadeScreen;
        if (sr.framebuffer.empty() || !target.ready())
            return;

        const auto edge = [](float px, float py, const SoftwareTextureVertex& v0, const SoftwareTextureVertex& v1) noexcept
        {
            return ((px - v0.x) * (v1.y - v0.y)) - ((py - v0.y) * (v1.x - v0.x));
        };
        const float area = edge(a.x, a.y, b, c);
        if (std::abs(area) <= 1.0e-4f)
            return;

        const int x0 = (std::max)(viewport.x, static_cast<int>(std::floor((std::min)({ a.x, b.x, c.x }))));
        const int x1 = (std::min)(viewport.x + viewport.width - 1, static_cast<int>(std::ceil((std::max)({ a.x, b.x, c.x }))));
        const int y0 = (std::max)(viewport.y, static_cast<int>(std::floor((std::min)({ a.y, b.y, c.y }))));
        const int y1 = (std::min)(viewport.y + viewport.height - 1, static_cast<int>(std::ceil((std::max)({ a.y, b.y, c.y }))));
        if (x0 > x1 || y0 > y1)
            return;

        const bool positive = area > 0.0f;
        const float invArea = 1.0f / area;
        for (int y = y0; y <= y1; ++y)
        {
            for (int x = x0; x <= x1; ++x)
            {
                const float sampleX = static_cast<float>(x) + 0.5f;
                const float sampleY = static_cast<float>(y) + 0.5f;
                const float wa = edge(sampleX, sampleY, b, c);
                const float wb = edge(sampleX, sampleY, c, a);
                const float wc = edge(sampleX, sampleY, a, b);
                if (!(positive ? (wa >= 0.0f && wb >= 0.0f && wc >= 0.0f)
                    : (wa <= 0.0f && wb <= 0.0f && wc <= 0.0f)))
                {
                    continue;
                }

                const float u = (std::clamp)((wa * a.u + wb * b.u + wc * c.u) * invArea, 0.0f, 1.0f);
                const float v = (std::clamp)((wa * a.v + wb * b.v + wc * c.v) * invArea, 0.0f, 1.0f);
                const int sourceX = (std::clamp)(
                    static_cast<int>(std::lround(u * static_cast<float>(target.width - 1))),
                    0,
                    target.width - 1);
                const int sourceY = (std::clamp)(
                    static_cast<int>(std::lround(v * static_cast<float>(target.height - 1))),
                    0,
                    target.height - 1);
                sr.framebuffer[
                    static_cast<std::size_t>(y) * static_cast<std::size_t>(sr.width)
                    + static_cast<std::size_t>(x)] = target.pixels[
                        static_cast<std::size_t>(sourceY) * static_cast<std::size_t>(target.width)
                        + static_cast<std::size_t>(sourceX)];
            }
        }
    }

    bool project_preview_vertex(
        const epochengine::previewgrid::Mat4& mvp,
        const epochengine::previewgrid::Vec3& position,
        const core::RenderViewport& viewport,
        float& outX,
        float& outY) noexcept
    {
        const auto clip = epochengine::previewgrid::transform_point(mvp, position);
        if (clip.w <= 1.0e-4f)
            return false;

        const float invW = 1.0f / clip.w;
        const float ndcX = clip.x * invW;
        const float ndcY = clip.y * invW;
        if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
            return false;

        outX = static_cast<float>(viewport.x)
            + ((ndcX * 0.5f) + 0.5f) * static_cast<float>(viewport.width);
        outY = static_cast<float>(viewport.y)
            + ((-ndcY * 0.5f) + 0.5f) * static_cast<float>(viewport.height);
        return true;
    }

    void render_engine_arcade_sampled_surface_preview(
        const core::Context& ctx,
        const previewgrid::Mat4& mvp,
        const core::RenderViewport& viewport,
        const std::shared_ptr<const epochengine::previewgrid::PublishedSceneFrame>& matchedScene) noexcept
    {
        const auto markers = matchedScene ? previewgrid::sampled_render_surface_markers_for(*matchedScene)
            : previewgrid::sampled_render_surface_markers_for(&ctx);
        if (markers.empty() || !prepare_arcade_screen_surface())
            return;

        for (const auto& marker : markers)
        {
            const float halfX = (std::max)(std::abs(marker.scale.x) * 0.5f, 0.25f);
            const float halfY = (std::max)(std::abs(marker.scale.y) * 0.5f, 0.18f);
            const float z =
                render_arcade::screen_sample_plane_z(
                    marker.position.z, marker.scale.z);
            const previewgrid::Vec3 world[4]{
                { marker.position.x - halfX, marker.position.y - halfY, z },
                { marker.position.x + halfX, marker.position.y - halfY, z },
                { marker.position.x + halfX, marker.position.y + halfY, z },
                { marker.position.x - halfX, marker.position.y + halfY, z }
            };
            SoftwareTextureVertex quad[4]{
                { 0.0f, 0.0f, 0.0f, 1.0f },
                { 0.0f, 0.0f, 1.0f, 1.0f },
                { 0.0f, 0.0f, 1.0f, 0.0f },
                { 0.0f, 0.0f, 0.0f, 0.0f }
            };
            bool visible = true;
            for (std::size_t index = 0; index < 4u; ++index)
            {
                if (!project_preview_vertex(
                        mvp,
                        world[index],
                        viewport,
                        quad[index].x,
                        quad[index].y))
                {
                    visible = false;
                    break;
                }
            }
            if (!visible)
                continue;

            draw_textured_triangle(quad[0], quad[1], quad[2], viewport);
            draw_textured_triangle(quad[0], quad[2], quad[3], viewport);
        }
    }

    bool render_scene_preview(const core::Context& ctx) noexcept
    {
        const auto requested = ctx.scene_viewport();
        const auto frame = rendercontext::resolve_frame_plan({
            ctx.framebufferWidth,
            ctx.framebufferHeight,
            { requested.x, requested.y, requested.width, requested.height },
            ctx.scene_preview_mode() == core::ScenePreviewMode::Editor,
            ctx.gui_overlay_priority(),
            ctx.frame_window_state()
        });
        if (!frame.scene_visible)
        {
            reset_canvas2d_scene_renderer();
            return false;
        }
        const core::RenderViewport viewport{
            frame.scene.x, frame.scene.y, frame.scene.width, frame.scene.height
        };

        if (render_canvas2d_scene(ctx, viewport))
            return true;

        const auto clearColor = epochengine::previewgrid::kClearColor;
        fill_preview_rect(
            viewport.x,
            viewport.y,
            viewport.width,
            viewport.height,
            pack_color(clearColor[0], clearColor[1], clearColor[2], clearColor[3]));

        const auto matchedScene = epochengine::previewgrid::scene_frame_for(&ctx);
        const auto camera = matchedScene ? matchedScene->camera : epochengine::previewgrid::camera_for(&ctx);
        const float aspect = viewport.height > 0
            ? (viewport.width / static_cast<float>(viewport.height))
            : 1.0f;
        const auto proj = epochengine::previewgrid::projection_for(&ctx, aspect, camera);
        const auto view = epochengine::previewgrid::look_at(
            camera.eye,
            camera.target,
            camera.up);
        const auto mvp = epochengine::previewgrid::multiply(proj, view);
        if (!matchedScene)
        {
            const auto gridGeometry = epochengine::previewgrid::grid_geometry_for(&ctx);
            const auto& vertices = gridGeometry->vertices;
            const auto& indices = gridGeometry->indices;

            for (std::size_t i = 0; i + 1 < indices.size(); i += 2)
            {
                const auto firstIndex = static_cast<std::size_t>(indices[i]);
                const auto secondIndex = static_cast<std::size_t>(indices[i + 1]);
                if (firstIndex >= vertices.size() || secondIndex >= vertices.size())
                    continue;

                float ax = 0.0f;
                float ay = 0.0f;
                float bx = 0.0f;
                float by = 0.0f;
                if (!project_preview_vertex(mvp, vertices[firstIndex].position, viewport, ax, ay)
                    || !project_preview_vertex(mvp, vertices[secondIndex].position, viewport, bx, by))
                {
                    continue;
                }

                const auto color = vertices[firstIndex].color;
                draw_line(
                    static_cast<int>(std::lround(ax)),
                    static_cast<int>(std::lround(ay)),
                    static_cast<int>(std::lround(bx)),
                    static_cast<int>(std::lround(by)),
                    pack_color(color.x, color.y, color.z, 1.0f),
                    viewport);
            }

        }

        const auto solidGeometry = matchedScene ? matchedScene->solids->geometry
            : epochengine::previewgrid::object_solid_geometry_for(&ctx);
        const auto meshWire = matchedScene ? matchedScene->wire
            : epochengine::previewgrid::object_mesh_wire_frame_for(&ctx);
        const auto& solidVertices = *solidGeometry;
        const auto* wireTriangles = meshWire->depth_triangles.get();
        const bool hasWireTriangles = wireTriangles && wireTriangles->size() >= 3;
        const bool depthReady = (solidVertices.size() >= 3 || hasWireTriangles) && prepare_preview_depth(viewport);
        for (std::size_t i = 0; depthReady && i + 2 < solidVertices.size(); i += 3)
        {
            if (!epochengine::previewgrid::clockwise_solid_triangle_faces_camera(
                    solidVertices[i].position,
                    solidVertices[i + 1].position,
                    solidVertices[i + 2].position,
                    camera.eye))
            {
                continue;
            }
            const auto color = solidVertices[i].color;
            draw_clipped_preview_triangle({
                previewgrid::transform_point(mvp, solidVertices[i].position),
                previewgrid::transform_point(mvp, solidVertices[i + 1].position),
                previewgrid::transform_point(mvp, solidVertices[i + 2].position)
            }, pack_color(color.x, color.y, color.z, 1.0f), viewport, camera.projection);
        }

        render_engine_arcade_sampled_surface_preview(ctx, mvp, viewport, matchedScene);

        if (!matchedScene)
        {
            const auto markerVertices = epochengine::previewgrid::look_marker_vertices_for(&ctx);
            const std::size_t markerCount = epochengine::previewgrid::look_marker_vertex_count_for(&ctx);
            for (std::size_t i = 0; i + 1 < markerCount; i += 2)
            {
                float ax = 0.0f;
                float ay = 0.0f;
                float bx = 0.0f;
                float by = 0.0f;
                if (!project_preview_vertex(mvp, markerVertices[i].position, viewport, ax, ay)
                    || !project_preview_vertex(mvp, markerVertices[i + 1].position, viewport, bx, by))
                {
                    continue;
                }

                const auto color = markerVertices[i].color;
                draw_line(
                    static_cast<int>(std::lround(ax)),
                    static_cast<int>(std::lround(ay)),
                    static_cast<int>(std::lround(bx)),
                    static_cast<int>(std::lround(by)),
                    pack_color(color.x, color.y, color.z, 1.0f),
                    viewport);
            }

        }

        if (hasWireTriangles && depthReady)
        {
            const auto drawWire = [&]<SoftwarePreviewRasterPass pass>()
            {
                const auto& triangles = *wireTriangles;
                for (std::size_t i = 0; i + 2 < triangles.size(); i += 3)
                {
                    const auto& a = triangles[i];
                    const auto& b = triangles[i + 1];
                    const auto& c = triangles[i + 2];
                    if (!previewgrid::clockwise_solid_triangle_faces_camera(a.position, b.position, c.position, camera.eye))
                        continue;
                    draw_clipped_preview_triangle<pass>({
                        previewgrid::transform_point(mvp, a.position),
                        previewgrid::transform_point(mvp, b.position),
                        previewgrid::transform_point(mvp, c.position)
                    }, pack_color(a.color.x, a.color.y, a.color.z, 1.0f), viewport, camera.projection);
                }
            };
            drawWire.operator()<SoftwarePreviewRasterPass::wire_depth>();
            drawWire.operator()<SoftwarePreviewRasterPass::wire_color>();
        }

        const auto objectGeometry = hasWireTriangles ? meshWire->helper_lines
            : (matchedScene ? matchedScene->lines : previewgrid::object_marker_geometry_for(&ctx));
        for (std::size_t i = 0; objectGeometry && i + 1 < objectGeometry->size(); i += 2)
        {
            const auto& first = (*objectGeometry)[i];
            const auto& second = (*objectGeometry)[i + 1];
            float ax = 0.0f;
            float ay = 0.0f;
            float bx = 0.0f;
            float by = 0.0f;
            if (!project_preview_vertex(mvp, first.position, viewport, ax, ay)
                || !project_preview_vertex(mvp, second.position, viewport, bx, by))
            {
                continue;
            }

            const auto color = first.color;
            draw_line(
                static_cast<int>(std::lround(ax)),
                static_cast<int>(std::lround(ay)),
                static_cast<int>(std::lround(bx)),
                static_cast<int>(std::lround(by)),
                pack_color(color.x, color.y, color.z, 1.0f),
                viewport);
        }

        // The fallback grid does not prove that the requested Canvas2D frame
        // was drawn. An ordinary visible spatial preview is ready here.
        return previewgrid::camera_mode_for(&ctx) != previewgrid::CameraMode::Canvas2D;
    }

#endif
}
namespace epochengine::anativecontext
{
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
    SoftwareCanvas2DContractFailure software_canvas2d_backend_contract_failure() noexcept
    {
        if (canvas2d::runtime::run_scene_raster_session_contract()
            != canvas2d::runtime::RuntimeContractFailure::none)
        {
            return SoftwareCanvas2DContractFailure::runtime_session;
        }

        constexpr std::uint32_t untouched = 0xAABBCCDDu;
        constexpr core::RenderViewport clippedViewport{-1, 1, 3, 2};
        const detail::Canvas2DBlitRegion clipped = detail::canvas2d_blit_region(
            4, 4, clippedViewport, 3, 2);
        if (clipped.destinationX != 0 || clipped.destinationY != 1
            || clipped.sourceX != 1 || clipped.sourceY != 0
            || clipped.width != 2 || clipped.height != 2)
        {
            return SoftwareCanvas2DContractFailure::clip_region;
        }
        if (detail::pack_canvas2d_pixel({0x11u, 0x22u, 0x33u, 0x44u})
            != 0x44112233u)
        {
            return SoftwareCanvas2DContractFailure::color_packing;
        }

        std::array<std::uint32_t, 16> framebuffer{};
        framebuffer.fill(untouched);
        constexpr std::array<canvas2d::cpu::Rgba8, 6> source{{
            {0x01u, 0x02u, 0x03u, 0x10u},
            {0x11u, 0x12u, 0x13u, 0x20u},
            {0x21u, 0x22u, 0x23u, 0x30u},
            {0x31u, 0x32u, 0x33u, 0x40u},
            {0x41u, 0x42u, 0x43u, 0x50u},
            {0x51u, 0x52u, 0x53u, 0x60u}
        }};
        if (!detail::blit_canvas2d_surface(
                framebuffer, 4, 4, source, 3, 2, clippedViewport)
            || framebuffer[4] != detail::blend_canvas2d_pixel(source[1], untouched)
            || framebuffer[5] != detail::blend_canvas2d_pixel(source[2], untouched)
            || framebuffer[8] != detail::blend_canvas2d_pixel(source[4], untouched)
            || framebuffer[9] != detail::blend_canvas2d_pixel(source[5], untouched)
            || framebuffer[3] != untouched
            || framebuffer[6] != untouched)
        {
            return SoftwareCanvas2DContractFailure::clipped_blit;
        }
        if ((framebuffer[4] >> 24u)
                != (detail::blend_canvas2d_pixel(source[1], untouched) >> 24u)
            || (framebuffer[8] >> 24u)
                != (detail::blend_canvas2d_pixel(source[4], untouched) >> 24u))
        {
            return SoftwareCanvas2DContractFailure::alpha_composition;
        }

        std::array<std::uint32_t, 15> resized{};
        resized.fill(untouched);
        constexpr std::array<canvas2d::cpu::Rgba8, 4> resizedSource{{
            {0x01u, 0x02u, 0x03u, 0x04u},
            {0x11u, 0x22u, 0x33u, 0x44u},
            {0x55u, 0x66u, 0x77u, 0x88u},
            {0x99u, 0xAAu, 0xBBu, 0xCCu}
        }};
        constexpr core::RenderViewport resizedViewport{3, 1, 2, 2};
        if (!detail::blit_canvas2d_surface(
                resized, 5, 3, resizedSource, 2, 2, resizedViewport)
            || resized[8] != detail::blend_canvas2d_pixel(resizedSource[0], untouched)
            || resized[9] != detail::blend_canvas2d_pixel(resizedSource[1], untouched)
            || resized[13] != detail::blend_canvas2d_pixel(resizedSource[2], untouched)
            || resized[14] != detail::blend_canvas2d_pixel(resizedSource[3], untouched)
            || resized[7] != untouched)
        {
            return SoftwareCanvas2DContractFailure::resize_blit;
        }
        return SoftwareCanvas2DContractFailure::none;
    }

    bool software_canvas2d_backend_contract() noexcept
    {
        return software_canvas2d_backend_contract_failure()
            == SoftwareCanvas2DContractFailure::none;
    }
#else
    SoftwareCanvas2DContractFailure software_canvas2d_backend_contract_failure() noexcept
    {
        return SoftwareCanvas2DContractFailure::none;
    }

    bool software_canvas2d_backend_contract() noexcept
    {
        return true;
    }
#endif
}
