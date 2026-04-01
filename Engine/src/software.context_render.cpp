module;

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <include/aengine.config.hpp>

module software.context;

import core.context;
import context.commandqueue;
import software.state;
import aengine.diagnostics;
import aengine.gui;
import aengine.telemetry;

namespace epochnamespace::anativecontext
{
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
    void capture_frame_if_requested(
        const std::vector<std::uint32_t>& framebuffer,
        int width,
        int height,
        std::uintptr_t windowId);

    namespace detail
    {
        void refresh_dimensions(core::Context& ctx) noexcept;
        void render_scene_preview(const core::Context& ctx) noexcept;
        bool same_viewport(
            const core::RenderViewport& lhs,
            const core::RenderViewport& rhs) noexcept;
        bool present_frame(const core::Context& ctx, const SoftRendState& sr) noexcept;
    }

    bool softrenderer_process(core::Context& ctx, core::CommandQueue& queue)
    {
        auto& sr = s_softrendererstate;
        detail::refresh_dimensions(ctx);

        std::uintptr_t windowId = 0;
#if defined(_WIN32)
        windowId = ctx.windowData
            ? reinterpret_cast<std::uintptr_t>(ctx.windowData->hwnd)
            : 0;
#endif

        diagnostics::FrameTiming frameTimer{ ctx.type, windowId, "Software" };
        const auto viewport = ctx.scene_viewport();
        const auto previewMode = ctx.scene_preview_mode();
        const std::uint64_t guiGeneration = epochnamespace::gui::deferred_batch_generation(&ctx);
        const bool hasPendingCommands = queue.depth() != 0;
        const bool sceneDirty =
            !sr.frameValid
            || hasPendingCommands
            || !detail::same_viewport(viewport, sr.lastSceneViewport)
            || static_cast<std::uint8_t>(previewMode) != sr.lastPreviewMode;
        const bool guiDirty =
            !sr.frameValid
            || guiGeneration != sr.lastGuiGeneration;

        if (sceneDirty)
        {
#if EPOCH_USE_CLEAR_COLOR
            const auto clearColor = core::clear_color_for_context(core::ContextType::Software);
            const auto clearR = static_cast<std::uint8_t>(
                std::clamp(clearColor[0], 0.0f, 1.0f) * 255.0f);
            const auto clearG = static_cast<std::uint8_t>(
                std::clamp(clearColor[1], 0.0f, 1.0f) * 255.0f);
            const auto clearB = static_cast<std::uint8_t>(
                std::clamp(clearColor[2], 0.0f, 1.0f) * 255.0f);
            const auto clearA = static_cast<std::uint8_t>(
                std::clamp(clearColor[3], 0.0f, 1.0f) * 255.0f);
            const std::uint32_t packedColor =
                (std::uint32_t(clearA) << 24)
                | (std::uint32_t(clearR) << 16)
                | (std::uint32_t(clearG) << 8)
                | std::uint32_t(clearB);
            std::fill(sr.framebuffer.begin(), sr.framebuffer.end(), packedColor);

            telemetry::emit_gauge(
                "renderer.framebuffer.size",
                static_cast<std::int64_t>(sr.width),
                telemetry::RendererTelemetryTags{ ctx.type, windowId, "width" });
            telemetry::emit_gauge(
                "renderer.framebuffer.size",
                static_cast<std::int64_t>(sr.height),
                telemetry::RendererTelemetryTags{ ctx.type, windowId, "height" });
            telemetry::emit_gauge(
                "renderer.framebuffer.size",
                static_cast<std::int64_t>(sr.framebuffer.size()),
                telemetry::RendererTelemetryTags{ ctx.type, windowId, "buffer_length" });
#endif

            detail::render_scene_preview(ctx);

            telemetry::emit_gauge(
                "renderer.command_queue.depth",
                static_cast<std::int64_t>(queue.depth()),
                telemetry::RendererTelemetryTags{ ctx.type, windowId });
            queue.drain();
            sr.sceneFramebuffer = sr.framebuffer;
        }

        const bool needsPresent = sceneDirty || guiDirty;
        if (needsPresent)
        {
            if (!sceneDirty && sr.sceneFramebuffer.size() == sr.framebuffer.size())
                sr.framebuffer = sr.sceneFramebuffer;

            if (ctx.windowData && ctx.windowData->context)
            {
                if (auto liveContext = std::reinterpret_pointer_cast<epochnamespace::core::Context>(ctx.windowData->context))
                    ::epochnamespace::gui::render_deferred_batch(liveContext.get());
            }

            sr.lastGuiGeneration = guiGeneration;
            sr.lastSceneViewport = viewport;
            sr.lastPreviewMode = static_cast<std::uint8_t>(previewMode);
            sr.frameValid = true;
        }

        if (needsPresent)
            capture_frame_if_requested(sr.framebuffer, sr.width, sr.height, windowId);

        if (needsPresent)
            (void)detail::present_frame(ctx, sr);

        frameTimer.finish();
        return true;
    }
#endif
}
