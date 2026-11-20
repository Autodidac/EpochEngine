module;

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <source_location>
#include <string>
#include <vector>

#include <include/engine.config.hpp>

module software.context;

import core.context;
import core.env;
import core.logger;
import context.commandqueue;
import perf.tier;
import render.preview_grid;
import software.state;
import diagnostics.engine;
import gui.engine;
import telemetry.engine;

namespace epochengine::anativecontext
{
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
    void capture_frame_if_requested(
        const std::vector<std::uint32_t>& framebuffer,
        int width,
        int height,
        std::uintptr_t windowId,
        bool projectPreviewRendered);

    namespace detail
    {
        void refresh_dimensions(core::Context& ctx) noexcept;
        bool render_scene_preview(const core::Context& ctx) noexcept;
        bool present_frame(const core::Context& ctx, const SoftRendState& sr) noexcept;

        using PhaseClock = std::chrono::steady_clock;
        using PhaseDurations = std::array<PhaseClock::duration, 5>;

        struct FramePhaseEvidence final
        {
            const core::Context* context{};
            PhaseClock::time_point sampleStart{};
            PhaseDurations totals{};
            std::uint64_t frames{};
            unsigned reports{};
            int width{}, height{};

            void reset_sample() noexcept
            {
                sampleStart = {};
                totals = {};
                frames = 0;
            }

            void observe(const core::Context& ctx, int currentWidth, int currentHeight,
                PhaseClock::time_point started, PhaseClock::time_point finished,
                const PhaseDurations& phases)
            {
                if (context != &ctx)
                {
                    *this = {};
                    context = &ctx;
                }
                if (reports >= 12u) return;
                if (ctx.frame_pacing_activity() != perf::frame_activity::foreground)
                {
                    reset_sample();
                    return;
                }
                if (width != currentWidth || height != currentHeight)
                {
                    reset_sample();
                    width = currentWidth;
                    height = currentHeight;
                }
                if (frames == 0) sampleStart = started;
                for (std::size_t phase = 0; phase < phases.size(); ++phase)
                    totals[phase] += phases[phase];
                ++frames;

                const auto elapsed = finished - sampleStart;
                if (elapsed < std::chrono::seconds(5)) return;
                const auto average_ms = [&](std::size_t phase) {
                    return std::chrono::duration<double, std::milli>(totals[phase]).count() / static_cast<double>(frames);
                };
                logger::get("Epoch.Software").logf(logger::LogLevel::INFO,
                    std::source_location::current(),
                    "{}x{} {:.1f} FPS ({} frames), ms: refresh/clear {:.3f}, scene {:.3f}, GUI/queue {:.3f}, capture {:.3f}, present {:.3f}",
                    width, height, static_cast<double>(frames) / std::chrono::duration<double>(elapsed).count(),
                    frames, average_ms(0), average_ms(1), average_ms(2), average_ms(3), average_ms(4));
                ++reports;
                reset_sample();
            }
        };
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
        static const bool profileFrames = [] {
            const auto value = core::env::get("EPOCH_SOFTWARE_FRAME_TIMING");
            return value && value->size() == 1u && value->data()[0] == '1';
        }();
        static thread_local detail::FramePhaseEvidence evidence{};
        const bool profile = profileFrames && (evidence.context != &ctx || evidence.reports < 12u);
        const auto phaseStamp = [profile] {
            return profile ? detail::PhaseClock::now() : detail::PhaseClock::time_point{};
        };
        const auto started = phaseStamp();
        detail::refresh_dimensions(ctx);

        std::uintptr_t windowId = 0;
#if defined(_WIN32)
        windowId = ctx.windowData
            ? reinterpret_cast<std::uintptr_t>(ctx.windowData->hwnd)
            : 0;
#endif

        diagnostics::FrameTiming frameTimer{ ctx.type, windowId, "Software" };
        const std::int64_t commandDepth = static_cast<std::int64_t>(queue.depth());
        sr.arcadeScreen.preparedThisFrame = false;
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

        const auto clearEnd = phaseStamp();
        bool guiRendered = false;
        if (!ctx.gui_overlay_priority()
            && ctx.windowData
            && ctx.windowData->context)
        {
            if (auto liveContext = std::reinterpret_pointer_cast<epochengine::core::Context>(
                    ctx.windowData->context))
            {
                guiRendered = ::epochengine::gui::render_deferred_batch(liveContext.get());
            }
        }
        const auto sceneStart = phaseStamp();
        const bool projectPreviewRendered = detail::render_scene_preview(ctx);
        const auto sceneEnd = phaseStamp();

        if (sr.lastTelemetryCommandDepth != commandDepth)
        {
            telemetry::emit_gauge(
                "renderer.command_queue.depth",
                commandDepth,
                telemetry::RendererTelemetryTags{ ctx.type, windowId });
            sr.lastTelemetryCommandDepth = commandDepth;
        }
        queue.drain();

        if (ctx.windowData && ctx.windowData->context)
        {
            if (auto liveContext = std::reinterpret_pointer_cast<epochengine::core::Context>(ctx.windowData->context))
                guiRendered |= ::epochengine::gui::render_deferred_batch(liveContext.get());
        }

        if (ctx.windowData && ctx.windowData->context)
        {
            if (auto liveContext = std::reinterpret_pointer_cast<epochengine::core::Context>(ctx.windowData->context))
                guiRendered |= ::epochengine::gui::render_top_layer_batch(liveContext.get());
        }

        const auto captureStart = phaseStamp();
        const bool projectContentRendered = projectPreviewRendered
            || (guiRendered && ctx.scene_preview_mode() == core::ScenePreviewMode::None
                && previewgrid::camera_mode_for(&ctx) != previewgrid::CameraMode::Canvas2D);
        capture_frame_if_requested(sr.framebuffer, sr.width, sr.height, windowId, projectContentRendered);
        const auto presentStart = phaseStamp();

        const bool presented = detail::present_frame(ctx, sr);
        const auto finished = phaseStamp();

        frameTimer.finish();
        if (profile && presented)
        {
            evidence.observe(ctx, sr.width, sr.height, started, finished,
                {clearEnd - started, sceneEnd - sceneStart,
                    (sceneStart - clearEnd) + (captureStart - sceneEnd),
                    presentStart - captureStart, finished - presentStart});
        }
        return true;
    }
#endif
}
