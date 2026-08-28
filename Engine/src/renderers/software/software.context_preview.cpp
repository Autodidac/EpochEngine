module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <span>

#include <include/engine.config.hpp>

module software.context;

import core.context;
import software.state;
import package.registry;
import render.arcade;
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
        [[nodiscard]] bool scene_changed(
            const core::Context& ctx,
            const core::RenderViewport& viewport) noexcept
        {
            std::scoped_lock lock(mutex_);
            if (!viewport.valid()
                || ctx.scene_preview_mode() != core::ScenePreviewMode::Editor)
            {
                return deactivate_locked();
            }

            const canvas2d::runtime::PreparedSceneView prepared =
                session_.prepare(
                    &ctx,
                    {
                        static_cast<std::uint32_t>(viewport.width),
                        static_cast<std::uint32_t>(viewport.height)},
                    execution_limits_.canvas,
                    execution_limits_.raster);
            if (prepared.code == canvas2d::runtime::PrepareCode::ready)
            {
                active_ = true;
                return true;
            }
            if (prepared.code == canvas2d::runtime::PrepareCode::reused)
            {
                active_ = true;
                return false;
            }
            if (prepared.code == canvas2d::runtime::PrepareCode::missing_scene)
                return deactivate_locked();

            const bool changed = active_ || prepared.code
                != canvas2d::runtime::PrepareCode::missing_scene;
            active_ = false;
            session_.reset();
            return changed;
        }

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
                active_ = false;
                return false;
            }

            const auto& presentation = prepared.raster->presentation;
            active_ = true;
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
            active_ = false;
        }

    private:
        [[nodiscard]] bool deactivate_locked() noexcept
        {
            const bool changed = active_;
            if (active_)
                session_.reset();
            active_ = false;
            return changed;
        }

        std::mutex mutex_{};
        canvas2d::limits::NativeExecutionLimits execution_limits_{
            canvas2d::limits::for_backend(RendererBackendKind::software)};
        canvas2d::runtime::SceneRasterSession session_{};
        bool active_{};
    };

    [[nodiscard]] SoftwareCanvas2DPresenter& canvas2d_presenter() noexcept
    {
        static SoftwareCanvas2DPresenter presenter{};
        return presenter;
    }

    [[nodiscard]] bool canvas2d_scene_changed(
        const core::Context& ctx,
        const core::RenderViewport& viewport) noexcept
    {
        return canvas2d_presenter().scene_changed(ctx, viewport);
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

    void draw_triangle(
        float ax,
        float ay,
        float bx,
        float by,
        float cx,
        float cy,
        std::uint32_t color,
        const core::RenderViewport& viewport) noexcept
    {
        auto& sr = s_softrendererstate;
        if (sr.framebuffer.empty() || sr.width <= 0 || sr.height <= 0)
            return;

        const auto edge = [](float px, float py, float x0, float y0, float x1, float y1) noexcept
        {
            return ((px - x0) * (y1 - y0)) - ((py - y0) * (x1 - x0));
        };

        const float area = edge(ax, ay, bx, by, cx, cy);
        if (std::abs(area) <= 1.0e-4f)
            return;

        const float minXf = (std::min)(ax, (std::min)(bx, cx));
        const float maxXf = (std::max)(ax, (std::max)(bx, cx));
        const float minYf = (std::min)(ay, (std::min)(by, cy));
        const float maxYf = (std::max)(ay, (std::max)(by, cy));

        const int x0 = (std::max)(viewport.x, static_cast<int>(std::floor(minXf)));
        const int x1 = (std::min)(viewport.x + viewport.width - 1, static_cast<int>(std::ceil(maxXf)));
        const int y0 = (std::max)(viewport.y, static_cast<int>(std::floor(minYf)));
        const int y1 = (std::min)(viewport.y + viewport.height - 1, static_cast<int>(std::ceil(maxYf)));
        if (x0 > x1 || y0 > y1)
            return;

        const bool positive = area > 0.0f;
        for (int py = y0; py <= y1; ++py)
        {
            for (int px = x0; px <= x1; ++px)
            {
                const float sampleX = static_cast<float>(px) + 0.5f;
                const float sampleY = static_cast<float>(py) + 0.5f;
                const float w0 = edge(sampleX, sampleY, bx, by, cx, cy);
                const float w1 = edge(sampleX, sampleY, cx, cy, ax, ay);
                const float w2 = edge(sampleX, sampleY, ax, ay, bx, by);
                if (positive ? (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) : (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f))
                {
                    sr.framebuffer[
                        static_cast<std::size_t>(py) * static_cast<std::size_t>(sr.width)
                        + static_cast<std::size_t>(px)] = color;
                }
            }
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
        const core::RenderViewport& viewport) noexcept
    {
        const auto markers = previewgrid::sampled_render_surface_markers_for(&ctx);
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

    void render_scene_preview(const core::Context& ctx) noexcept
    {
        const auto requested = ctx.scene_viewport();
        const auto frame = rendercontext::resolve_frame_plan({
            ctx.framebufferWidth,
            ctx.framebufferHeight,
            { requested.x, requested.y, requested.width, requested.height },
            ctx.scene_preview_mode() == core::ScenePreviewMode::Editor,
            ctx.gui_overlay_priority()
        });
        if (!frame.scene_visible)
        {
            reset_canvas2d_scene_renderer();
            return;
        }
        const core::RenderViewport viewport{
            frame.scene.x, frame.scene.y, frame.scene.width, frame.scene.height
        };

        if (render_canvas2d_scene(ctx, viewport))
            return;

        const auto clearColor = epochengine::previewgrid::kClearColor;
        fill_preview_rect(
            viewport.x,
            viewport.y,
            viewport.width,
            viewport.height,
            pack_color(clearColor[0], clearColor[1], clearColor[2], clearColor[3]));

        const auto camera = epochengine::previewgrid::camera_for(&ctx);
        const float aspect = viewport.height > 0
            ? (viewport.width / static_cast<float>(viewport.height))
            : 1.0f;
        const auto proj = epochengine::previewgrid::projection_for(&ctx, aspect, camera);
        const auto view = epochengine::previewgrid::look_at(
            camera.eye,
            camera.target,
            camera.up);
        const auto mvp = epochengine::previewgrid::multiply(proj, view);
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

        const auto solidVertices = epochengine::previewgrid::object_solid_vertices_for(&ctx);
        for (std::size_t i = 0; i + 2 < solidVertices.size(); i += 3)
        {
            if (!epochengine::previewgrid::clockwise_solid_triangle_faces_camera(
                    solidVertices[i].position,
                    solidVertices[i + 1].position,
                    solidVertices[i + 2].position,
                    camera.eye))
            {
                continue;
            }
            float ax = 0.0f;
            float ay = 0.0f;
            float bx = 0.0f;
            float by = 0.0f;
            float cx = 0.0f;
            float cy = 0.0f;
            if (!project_preview_vertex(mvp, solidVertices[i].position, viewport, ax, ay)
                || !project_preview_vertex(mvp, solidVertices[i + 1].position, viewport, bx, by)
                || !project_preview_vertex(mvp, solidVertices[i + 2].position, viewport, cx, cy))
            {
                continue;
            }

            const auto color = solidVertices[i].color;
            draw_triangle(
                ax,
                ay,
                bx,
                by,
                cx,
                cy,
                pack_color(color.x, color.y, color.z, 1.0f),
                viewport);
        }

        render_engine_arcade_sampled_surface_preview(ctx, mvp, viewport);

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

        const auto objectVertices = epochengine::previewgrid::object_marker_vertices_for(&ctx);
        for (std::size_t i = 0; i + 1 < objectVertices.size(); i += 2)
        {
            float ax = 0.0f;
            float ay = 0.0f;
            float bx = 0.0f;
            float by = 0.0f;
            if (!project_preview_vertex(mvp, objectVertices[i].position, viewport, ax, ay)
                || !project_preview_vertex(mvp, objectVertices[i + 1].position, viewport, bx, by))
            {
                continue;
            }

            const auto color = objectVertices[i].color;
            draw_line(
                static_cast<int>(std::lround(ax)),
                static_cast<int>(std::lround(ay)),
                static_cast<int>(std::lround(bx)),
                static_cast<int>(std::lround(by)),
                pack_color(color.x, color.y, color.z, 1.0f),
                viewport);
        }
    }

    bool same_viewport(const core::RenderViewport& lhs, const core::RenderViewport& rhs) noexcept
    {
        return lhs.x == rhs.x
            && lhs.y == rhs.y
            && lhs.width == rhs.width
            && lhs.height == rhs.height;
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
