module;

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <include/aengine.config.hpp>

module software.context;

import core.context;
import software.state;
import render.preview_grid;

namespace epochnamespace::anativecontext::detail
{
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
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

    void draw_line(int x0, int y0, int x1, int y1, std::uint32_t color) noexcept
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
            if (x0 >= 0 && x0 < sr.width && y0 >= 0 && y0 < sr.height)
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

    bool project_preview_vertex(
        const epochnamespace::previewgrid::Mat4& mvp,
        const epochnamespace::previewgrid::Vec3& position,
        const core::RenderViewport& viewport,
        float& outX,
        float& outY) noexcept
    {
        const auto clip = epochnamespace::previewgrid::transform_point(mvp, position);
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

    void render_scene_preview(const core::Context& ctx) noexcept
    {
        const auto viewport = ctx.scene_viewport();
        if (!viewport.valid() || ctx.scene_preview_mode() != core::ScenePreviewMode::Editor)
            return;

        const auto clearColor = epochnamespace::previewgrid::kClearColor;
        fill_preview_rect(
            viewport.x,
            viewport.y,
            viewport.width,
            viewport.height,
            pack_color(clearColor[0], clearColor[1], clearColor[2], clearColor[3]));

        const auto camera = epochnamespace::previewgrid::camera_for(&ctx);
        const float aspect = viewport.height > 0
            ? (viewport.width / static_cast<float>(viewport.height))
            : 1.0f;
        const auto proj = epochnamespace::previewgrid::perspective(
            camera.fovRadians,
            aspect,
            camera.nearPlane,
            camera.farPlane);
        const auto view = epochnamespace::previewgrid::look_at(
            camera.eye,
            camera.target,
            camera.up);
        const auto mvp = epochnamespace::previewgrid::multiply(proj, view);
        const auto vertices = epochnamespace::previewgrid::grid_vertices();
        const auto indices = epochnamespace::previewgrid::grid_indices();

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
                pack_color(color.x, color.y, color.z, 1.0f));
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
