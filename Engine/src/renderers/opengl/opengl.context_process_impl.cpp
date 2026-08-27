#include <algorithm>
#include <chrono>
#include <source_location>

#include <include/engine.config.hpp>

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
#   include <glad/glad.h>
#endif

#include "opengl.context_detail.hpp"
#include "opengl.frame_capture.hpp"
#include "opengl.context_process_impl.hpp"

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import core.context;
import core.logger;
import telemetry.engine;
import context.commandqueue;
import opengl.context;
import opengl.platform;
import opengl.quad;
import opengl.state;
import opengl.textures;

namespace epochengine::openglcontext
{
    bool process_impl(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        using FrameClock = std::chrono::steady_clock;
        const auto framePhaseStart = FrameClock::now();

        if (!ctx)
            return false;

        if (ctx->windowData && ctx->windowData->get_should_close())
        {
            queue.clear();
            return false;
        }

        std::uintptr_t windowId = 0u;
#if defined(_WIN32)
        if (ctx->windowData)
            windowId = reinterpret_cast<std::uintptr_t>(ctx->windowData->hwnd);
        else if (ctx->hwnd)
            windowId = reinterpret_cast<std::uintptr_t>(ctx->hwnd);
#endif

        auto& backend = opengltextures::get_opengl_backend();
        auto& glState = backend.glState;

        if (!contextdetail::drawable_alive(ctx.get(), glState))
            return false;

        PlatformGL::ScopedContext guard;
        auto desired = contextdetail::context_to_platform_context(ctx.get());
        if (!desired.valid() || !guard.set(desired))
        {
            auto fallback = contextdetail::state_to_platform_context(glState);
            if (!fallback.valid() || !guard.set(fallback))
                return false;
        }

        const auto previousRenderContext = core::get_current_render_context();
        core::set_current_render_context(ctx);
        struct ResetRenderContext final
        {
            std::shared_ptr<core::Context> previous{};
            ~ResetRenderContext()
            {
                core::set_current_render_context(std::move(previous));
            }
        } resetRenderContext{ previousRenderContext };

        int fbW = (std::max)(1, opengl_get_width());
        int fbH = (std::max)(1, opengl_get_height());

#if defined(_WIN32)
        if (auto size = contextdetail::query_drawable_size(ctx.get(), glState); size.valid())
        {
            fbW = size.width;
            fbH = size.height;
        }
#elif defined(__linux__)
        if (auto size = contextdetail::query_drawable_size(guard.target(), glState); size.valid())
        {
            fbW = size.width;
            fbH = size.height;
        }
#endif

        glState.width = static_cast<unsigned int>((std::max)(1, fbW));
        glState.height = static_cast<unsigned int>((std::max)(1, fbH));
        ctx->width = fbW;
        ctx->height = fbH;
        ctx->framebufferWidth = fbW;
        ctx->framebufferHeight = fbH;

        glViewport(0, 0, fbW, fbH);

        if (!openglquad::ensure_quad_pipeline(glState))
        {
            return true;
        }

        opengl_clear();
        opengl_render_active_frame(ctx, queue, fbW, fbH, windowId);
        if (ctx->windowData && ctx->windowData->get_should_close())
            return false;
        const auto presentStart = FrameClock::now();
        openglcapture::capture_frame_if_requested(
            fbW, fbH, windowId);
        PlatformGL::swap_buffers(guard.target());
        const auto presentEnd = FrameClock::now();

        const double prepareAndDrawMs =
            std::chrono::duration<double, std::milli>(
                presentStart - framePhaseStart).count();
        const double presentMs =
            std::chrono::duration<double, std::milli>(
                presentEnd - presentStart).count();
        const telemetry::RendererTelemetryTags timingTags{
            ctx->type,
            windowId};
        telemetry::emit_histogram_ms(
            "renderer.opengl.prepare_draw_time_ms",
            prepareAndDrawMs,
            timingTags);
        telemetry::emit_histogram_ms(
            "renderer.opengl.swap_buffers_time_ms",
            presentMs,
            timingTags);

        struct BoundedPhaseEvidence final
        {
            const core::Context* owner{};
            FrameClock::time_point sampleStart{};
            double prepareAndDrawTotalMs{};
            double presentTotalMs{};
            std::uint64_t samples{};
            std::uint32_t reports{};
        };
        static thread_local BoundedPhaseEvidence phaseEvidence{};
        if (phaseEvidence.owner != ctx.get())
        {
            phaseEvidence = {};
            phaseEvidence.owner = ctx.get();
            phaseEvidence.sampleStart = framePhaseStart;
        }
        phaseEvidence.prepareAndDrawTotalMs += prepareAndDrawMs;
        phaseEvidence.presentTotalMs += presentMs;
        ++phaseEvidence.samples;
        if (phaseEvidence.reports < 6u
            && presentEnd - phaseEvidence.sampleStart
                >= std::chrono::seconds(1))
        {
            logger::get("OpenGL").logf(
                logger::LogLevel::INFO,
                std::source_location::current(),
                "Frame phases: prepare+draw {:.3f} ms | SwapBuffers {:.3f} ms | {} frames.",
                phaseEvidence.prepareAndDrawTotalMs
                    / static_cast<double>(phaseEvidence.samples),
                phaseEvidence.presentTotalMs
                    / static_cast<double>(phaseEvidence.samples),
                phaseEvidence.samples);
            phaseEvidence.sampleStart = presentEnd;
            phaseEvidence.prepareAndDrawTotalMs = 0.0;
            phaseEvidence.presentTotalMs = 0.0;
            phaseEvidence.samples = 0u;
            ++phaseEvidence.reports;
        }

        ++glState.frameCount;

#if defined(_WIN32)
        if (ctx->windowData
            && !ctx->windowData->firstPresentComplete.exchange(true, std::memory_order_acq_rel)
            && ctx->windowData->hwnd
            && ::IsWindow(ctx->windowData->hwnd) != FALSE)
        {
            ::RedrawWindow(
                ctx->windowData->hwnd,
                nullptr,
                nullptr,
                RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        }
#else
        if (ctx->windowData
            && !ctx->windowData->firstPresentComplete.exchange(true, std::memory_order_acq_rel))
        {
        }
#endif
        return true;
    }
}
#endif
