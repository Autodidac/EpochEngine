module;

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <include/engine.config.hpp>

module software.context;

import core.context;
import core.commandline;
import context.commandqueue;
import software.state;
import engine.diagnostics;
import engine.gui;
import engine.telemetry;
import render.preview_grid;

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
        const bool closeRequested =
            !sr.running
            || (ctx.windowData && ctx.windowData->get_should_close());
        if (closeRequested)
        {
            sr.running = false;
            queue.clear();
            return false;
        }
        if (core::cli::smoke_requested && !core::cli::capture_requested)
        {
            ++sr.smokeFrames;
            if (sr.smokeFrames >= 3u)
            {
                sr.running = false;
                if (ctx.windowData)
                    ctx.windowData->set_should_close(true);
                queue.clear();
                return false;
            }
        }

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
        const std::uint64_t cameraRevision =
            previewMode == core::ScenePreviewMode::Editor
            ? epochnamespace::previewgrid::camera_revision_for(&ctx)
            : 0;
        const std::uint64_t guiGeneration = epochnamespace::gui::deferred_batch_generation(&ctx);
        const std::int64_t commandDepth = static_cast<std::int64_t>(queue.depth());
        const bool hasPendingCommands = commandDepth != 0;
        const bool sceneDirty =
            !sr.frameValid
            || hasPendingCommands
            || !detail::same_viewport(viewport, sr.lastSceneViewport)
            || static_cast<std::uint8_t>(previewMode) != sr.lastPreviewMode
            || cameraRevision != sr.lastCameraRevision;
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

            if (sr.lastTelemetryWidth != sr.width)
            {
                telemetry::emit_gauge(
                    "renderer.framebuffer.size",
                    static_cast<std::int64_t>(sr.width),
                    telemetry::RendererTelemetryTags{ ctx.type, windowId, "width" });
                sr.lastTelemetryWidth = sr.width;
            }
            if (sr.lastTelemetryHeight != sr.height)
            {
                telemetry::emit_gauge(
                    "renderer.framebuffer.size",
                    static_cast<std::int64_t>(sr.height),
                    telemetry::RendererTelemetryTags{ ctx.type, windowId, "height" });
                sr.lastTelemetryHeight = sr.height;
            }
            if (sr.lastTelemetryBufferLength != sr.framebuffer.size())
            {
                telemetry::emit_gauge(
                    "renderer.framebuffer.size",
                    static_cast<std::int64_t>(sr.framebuffer.size()),
                    telemetry::RendererTelemetryTags{ ctx.type, windowId, "buffer_length" });
                sr.lastTelemetryBufferLength = sr.framebuffer.size();
            }
#endif

            detail::render_scene_preview(ctx);

            if (sr.lastTelemetryCommandDepth != commandDepth)
            {
                telemetry::emit_gauge(
                    "renderer.command_queue.depth",
                    commandDepth,
                    telemetry::RendererTelemetryTags{ ctx.type, windowId });
                sr.lastTelemetryCommandDepth = commandDepth;
            }
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

            detail::render_scene_preview(ctx);

            sr.lastGuiGeneration = guiGeneration;
            sr.lastCameraRevision = cameraRevision;
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
