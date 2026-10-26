#include <algorithm>
#include <chrono>
#include <source_location>
#include <stdexcept>

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
#   include "opengl.frame_admission.hpp"
#endif

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import core.context;
import core.commandline;
import core.logger;
import telemetry.engine;
import context.commandqueue;
import opengl.context;
import opengl.platform;
import opengl.quad;
import opengl.state;
import opengl.textures;
import perf.tier;

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
        auto desired = contextdetail::context_to_platform_context(ctx.get());
        auto& glState = desired.valid()
            ? opengltextures::state_for_platform_context(backend, desired, ctx.get())
            : backend.glState;

        if (!contextdetail::drawable_alive(ctx.get(), glState))
            return false;

        PlatformGL::ScopedContext guard;
        if (!desired.valid() || !guard.set(desired))
        {
            auto fallback = contextdetail::state_to_platform_context(glState);
            if (!fallback.valid() || !guard.set(fallback))
                return false;
        }

        auto& nativeState = opengltextures::state_for_platform_context(
            backend, guard.target(), ctx.get());
        const bool uncappedSubmission = ctx->windowData
            ? ctx->windowData->uncappedFrameSubmission.load(std::memory_order_relaxed)
                && !(ctx->windowData->editorThrottleHz.load(std::memory_order_relaxed) > 0.0)
            // Direct legacy callers have no published host plan. Bound only an
            // explicit foreground uncapped request; leave driver VSync alone.
            : core::cli::frame_limit_explicit && !core::cli::frame_vsync_requested
                && core::cli::frame_limit_fps <= 0.0
                && ctx->frame_pacing_activity() == perf::frame_activity::foreground;
        const auto admissionStart = FrameClock::now();
        std::uint32_t admissionWaits{};
        if (uncappedSubmission)
        {
            const auto admitted = frame_admission::admit(nativeState.frameAdmission, [&]() noexcept
            {
                return !ctx->windowData
                    || (ctx->windowData->running.load(std::memory_order_acquire)
                        && !ctx->windowData->get_should_close());
            }, ctx->windowData
                ? (std::numeric_limits<std::uint32_t>::max)() : 2'000u);
            admissionWaits = admitted.blocking_waits;
            if (admitted.status == frame_admission::AdmissionStatus::cancelled)
                return false;
            if (admitted.status == frame_admission::AdmissionStatus::failed)
                throw std::runtime_error("OpenGL GPU frame admission failed or exceeded the direct-call wait budget; no additional frame was submitted.");
        }
        else if (!frame_admission::release(nativeState.frameAdmission))
            throw std::runtime_error("OpenGL GPU frame admission cleanup is unavailable.");
        const auto admissionEnd = FrameClock::now();

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
            return false;
        }

        opengl_clear();
        nativeState.guiSpritesSubmitted = nativeState.guiDrawCallsSubmitted = 0;
        opengl_render_active_frame(ctx, queue, fbW, fbH, windowId);
        if (ctx->windowData && ctx->windowData->get_should_close())
            return false;
        const auto presentStart = FrameClock::now();
        openglcapture::capture_frame_if_requested(
            fbW, fbH, windowId);
        // Uncapped means each completed render is submitted immediately.
        // Do not conceal a native presentation ceiling behind a render counter.
        const bool nativeSucceeded = PlatformGL::swap_buffers(guard.target());
        const auto presentEnd = FrameClock::now();
        if (uncappedSubmission && !frame_admission::submitted(nativeState.frameAdmission))
            throw std::runtime_error("OpenGL GPU completion fence creation failed; rendering stopped before another submission.");

        const double admissionMs = std::chrono::duration<double, std::milli>(
            admissionEnd - admissionStart).count();
        const double prepareAndDrawMs =
            std::chrono::duration<double, std::milli>(
                presentStart - framePhaseStart).count() - admissionMs;
        const double presentMs =
            std::chrono::duration<double, std::milli>(
                presentEnd - presentStart).count();
        const telemetry::RendererTelemetryTags timingTags{
            ctx->type,
            windowId};
        telemetry::emit_histogram_ms(
            "renderer.opengl.gpu_admission_wait_time_ms",
            admissionMs,
            timingTags);
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
            bool uncapped{};
            FrameClock::time_point sampleStart{};
            double admissionTotalMs{};
            std::uint64_t admissionWaits{};
            double prepareAndDrawTotalMs{};
            double presentTotalMs{};
            std::uint64_t samples{};
            std::uint64_t guiSprites{}, guiDrawCalls{};
            std::uint32_t reports{};
        };
        static thread_local BoundedPhaseEvidence phaseEvidence{};
        if (phaseEvidence.owner != ctx.get() || phaseEvidence.uncapped != uncappedSubmission)
        {
            phaseEvidence = {};
            phaseEvidence.owner = ctx.get();
            phaseEvidence.uncapped = uncappedSubmission;
            phaseEvidence.sampleStart = framePhaseStart;
            logger::get("OpenGL").logf(
                logger::LogLevel::INFO,
                std::source_location::current(),
                "GPU frame admission {}: maximum {} submitted frames pending completion; no FPS ceiling.",
                uncappedSubmission ? "enabled" : "inactive",
                frame_admission::frames_in_flight);
        }
        phaseEvidence.admissionTotalMs += admissionMs;
        phaseEvidence.admissionWaits += admissionWaits;
        phaseEvidence.prepareAndDrawTotalMs += prepareAndDrawMs;
        phaseEvidence.presentTotalMs += presentMs;
        ++phaseEvidence.samples;
        phaseEvidence.guiSprites += nativeState.guiSpritesSubmitted;
        phaseEvidence.guiDrawCalls += nativeState.guiDrawCallsSubmitted;
        if (phaseEvidence.reports < 6u
            && presentEnd - phaseEvidence.sampleStart
                >= std::chrono::seconds(1))
        {
            logger::get("OpenGL").logf(
                logger::LogLevel::INFO,
                std::source_location::current(),
                "Frame phases: prepare+draw {:.3f} ms | GPU admission {:.3f} ms ({} driver waits) | SwapBuffers {:.3f} ms | {} rendered frames; uncapped admission={}; GUI {:.1f} sprites in {:.1f} draw calls/frame.",
                phaseEvidence.prepareAndDrawTotalMs
                    / static_cast<double>(phaseEvidence.samples),
                phaseEvidence.admissionTotalMs
                    / static_cast<double>(phaseEvidence.samples),
                phaseEvidence.admissionWaits,
                phaseEvidence.presentTotalMs
                    / static_cast<double>(phaseEvidence.samples),
                phaseEvidence.samples,
                phaseEvidence.uncapped,
                static_cast<double>(phaseEvidence.guiSprites) / phaseEvidence.samples,
                static_cast<double>(phaseEvidence.guiDrawCalls) / phaseEvidence.samples);
            phaseEvidence.sampleStart = presentEnd;
            phaseEvidence.prepareAndDrawTotalMs = 0.0;
            phaseEvidence.admissionTotalMs = 0.0;
            phaseEvidence.admissionWaits = 0u;
            phaseEvidence.presentTotalMs = 0.0;
            phaseEvidence.samples = 0u;
            phaseEvidence.guiSprites = phaseEvidence.guiDrawCalls = 0;
            ++phaseEvidence.reports;
        }

        // The GPU fence above still owns this rendered work on swap failure.
        // A failed native submission must not advance completion/FPS evidence.
        if (!nativeSucceeded)
            return false;
        ++glState.frameCount;

#if defined(_WIN32)
        if (nativeSucceeded && ctx->windowData
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
        if (nativeSucceeded && ctx->windowData
            && !ctx->windowData->firstPresentComplete.exchange(true, std::memory_order_acq_rel))
        {
        }
#endif
        return true;
    }
}
#endif
